// BaseCharacter.cpp
#include "BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

// 로그 카테고리 정의
DEFINE_LOG_CATEGORY(LogBaseChar);

ABaseCharacter::ABaseCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 1. 메쉬 컴포넌트 초기화
    // GetMesh()는 ACharacter가 이미 가지고 있습니다. 이를 Root(Body)로 씁니다.

    // 머리
    HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
    HeadMesh->SetupAttachment(GetMesh()); // 몸체에 붙임
    HeadMesh->SetCollisionProfileName(TEXT("NoCollision")); // 충돌은 캡슐이 담당

    // 상체
    ChestMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMesh"));
    ChestMesh->SetupAttachment(GetMesh());
    ChestMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // 장갑
    HandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandMesh"));
    HandMesh->SetupAttachment(GetMesh());
    HandMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // 하의
    LegMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegMesh"));
    LegMesh->SetupAttachment(GetMesh());
    LegMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // 신발
    FootMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FootMesh"));
    FootMesh->SetupAttachment(GetMesh());
    FootMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // 2. 기본값 설정
    bEnableArmorStats = false; // 기본적으로 무게 시스템 활성화
    DefaultWalkSpeed = 600.0f;
    CurrentTotalWeight = 0.0f;
}

void ABaseCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 3. Leader Pose Component 설정 (매우 중요)
    // 모든 파츠가 Body(GetMesh)의 애니메이션을 따라하도록 설정합니다.
    // UE5에서는 SetLeaderPoseComponent, 구버전은 SetMasterPoseComponent

    HeadMesh->SetLeaderPoseComponent(GetMesh());
    ChestMesh->SetLeaderPoseComponent(GetMesh());
    HandMesh->SetLeaderPoseComponent(GetMesh());
    LegMesh->SetLeaderPoseComponent(GetMesh());
    FootMesh->SetLeaderPoseComponent(GetMesh());

    // 초기 이동 속도 저장
    if (GetCharacterMovement())
    {
        DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
    }

    CHAR_LOG(Display, TEXT("Character Ready. Stats Mode: %d"), bEnableArmorStats);
}

void ABaseCharacter::EquipArmor(EArmorSlot Slot, const FArmorData& NewArmor)
{
    USkeletalMeshComponent* TargetMesh = nullptr;

    // 슬롯에 맞는 메쉬 컴포넌트 선택
    switch (Slot)
    {
    case EArmorSlot::Head:  TargetMesh = HeadMesh; break;
    case EArmorSlot::Chest: TargetMesh = ChestMesh; break;
    case EArmorSlot::Hands: TargetMesh = HandMesh; break;
    case EArmorSlot::Legs:  TargetMesh = LegMesh; break;
    case EArmorSlot::Feet:  TargetMesh = FootMesh; break;
    default:
        CHAR_LOG(Warning, TEXT("Invalid Armor Slot requested"));
        return;
    }

    if (TargetMesh)
    {
        // 1. 메쉬 교체
        // NewArmor.ArmorMesh가 nullptr이면 장비 해제(맨몸)로 간주 가능
        TargetMesh->SetSkeletalMesh(NewArmor.ArmorMesh);

        CHAR_LOG(Log, TEXT("Equipped Armor on Slot [%d] : %s"), (uint8)Slot, *NewArmor.RowName.ToString());

        // 2. 무게 계산 로직
        if (bEnableArmorStats)
        {
            // 기존에 해당 슬롯에 있던 무게 제거
            if (EquippedArmorWeights.Contains(Slot))
            {
                CurrentTotalWeight -= EquippedArmorWeights[Slot];
            }

            // 새 무게 추가
            float NewWeight = NewArmor.WeightKg;
            EquippedArmorWeights.Add(Slot, NewWeight);
            CurrentTotalWeight += NewWeight;

            // 속도 업데이트
            UpdateMovementSpeedBasedOnWeight();
        }
    }
}

void ABaseCharacter::UpdateMovementSpeedBasedOnWeight()
{
    if (!GetCharacterMovement()) return;

    if (!bEnableArmorStats)
    {
        // 스탯 모드가 꺼져있으면 기본 속도 유지
        GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
        return;
    }

    // [공식 제안] 속도 패널티 = 무게 * 계수
    // 예: 1kg당 이동속도 5 감소
    float WeightPenalty = CurrentTotalWeight * 5.0f;

    // 최소 이동 속도는 150.0f으로 제한 (너무 느려져서 멈추는 것 방지)
    float NewSpeed = FMath::Clamp(DefaultWalkSpeed - WeightPenalty, 150.0f, DefaultWalkSpeed);

    GetCharacterMovement()->MaxWalkSpeed = NewSpeed;

    CHAR_LOG(Verbose, TEXT("Weight: %.1f kg -> Speed Updated: %.1f (Penalty: -%.1f)"),
        CurrentTotalWeight, NewSpeed, WeightPenalty);
}

void ABaseCharacter::SetArmorColor(EArmorSlot Slot, FLinearColor Color)
{
    USkeletalMeshComponent* TargetMesh = nullptr;
    switch (Slot)
    {
        case EArmorSlot::Head: TargetMesh = HeadMesh; break;
        case EArmorSlot::Chest: TargetMesh = ChestMesh; break;
        case EArmorSlot::Hands: TargetMesh = HandMesh; break;
        case EArmorSlot::Legs: TargetMesh = LegMesh; break;
        case EArmorSlot::Feet: TargetMesh = FootMesh; break;
        default: return;
    }

    if (TargetMesh)
    {
        // 첫 번째 머티리얼 인덱스(0)의 색상을 바꾼다고 가정
        // 실제로는 CreateDynamicMaterialInstance가 필요할 수 있음
        TargetMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(Color));
    }
}