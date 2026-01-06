// BaseCharacter.cpp
#include "BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "BaseWeapon.h" // [중요] BaseWeapon 기능을 사용하기 위해 Include

DEFINE_LOG_CATEGORY(LogBaseChar);

ABaseCharacter::ABaseCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 아머 메쉬 초기화
    HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
    HeadMesh->SetupAttachment(GetMesh());
    HeadMesh->SetCollisionProfileName(TEXT("NoCollision"));

    ChestMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMesh"));
    ChestMesh->SetupAttachment(GetMesh());
    ChestMesh->SetCollisionProfileName(TEXT("NoCollision"));

    HandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandMesh"));
    HandMesh->SetupAttachment(GetMesh());
    HandMesh->SetCollisionProfileName(TEXT("NoCollision"));

    LegMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegMesh"));
    LegMesh->SetupAttachment(GetMesh());
    LegMesh->SetCollisionProfileName(TEXT("NoCollision"));

    FootMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FootMesh"));
    FootMesh->SetupAttachment(GetMesh());
    FootMesh->SetCollisionProfileName(TEXT("NoCollision"));

    bEnableArmorStats = false;
    DefaultWalkSpeed = 600.0f;
    CurrentTotalWeight = 0.0f;
    CurrentWeapon = nullptr; // 무기 초기화
}

void ABaseCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Leader Pose 설정
    HeadMesh->SetLeaderPoseComponent(GetMesh());
    ChestMesh->SetLeaderPoseComponent(GetMesh());
    HandMesh->SetLeaderPoseComponent(GetMesh());
    LegMesh->SetLeaderPoseComponent(GetMesh());
    FootMesh->SetLeaderPoseComponent(GetMesh());

    if (GetCharacterMovement())
    {
        DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
    }
}

// [신규] 무기 장착 구현
void ABaseCharacter::EquipWeapon(ABaseWeapon* NewWeapon)
{
    if (!NewWeapon) return;

    // 기존 무기 버리기 (교체)
    if (CurrentWeapon)
    {
        DropCurrentWeapon();
    }

    CurrentWeapon = NewWeapon;

    // 1. 소유권 설정 (AI나 플레이어가 데미지를 입혔을 때 판정용)
    CurrentWeapon->SetOwner(this);

    // 2. 무기 상태 변경 (물리 끄기, 충돌 끄기 등)
    CurrentWeapon->OnEquipped();

    // 3. 소켓 부착
    FName SocketName = TEXT("WeaponSocket");

    // 장갑(HandMesh)이 있다면 거기에, 없으면 몸통(GetMesh)에 부착
    USkeletalMeshComponent* AttachTarget = HandMesh ? HandMesh : GetMesh();

    if (AttachTarget->DoesSocketExist(SocketName))
    {
        CurrentWeapon->AttachToComponent(AttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
    }
    else
    {
        // 소켓 없으면 임시로 오른손 뼈에 부착
        CurrentWeapon->AttachToComponent(AttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
        CHAR_LOG(Warning, TEXT("Socket 'WeaponSocket' not found. Attached to 'hand_r' instead."));
    }

    CHAR_LOG(Log, TEXT("Equipped Weapon: %s"), *NewWeapon->GetName());
}

// [신규] 무기 버리기 구현
void ABaseCharacter::DropCurrentWeapon()
{
    if (!CurrentWeapon) return;

    CHAR_LOG(Log, TEXT("Dropping Weapon: %s"), *CurrentWeapon->GetName());

    // 무기에게 드랍 신호 (물리 켜기, 분리)
    CurrentWeapon->OnDropped();

    // 참조 해제
    CurrentWeapon = nullptr;
}

void ABaseCharacter::EquipArmor(EArmorSlot Slot, const FArmorData& NewArmor)
{
    USkeletalMeshComponent* TargetMesh = nullptr;
    switch (Slot)
    {
    case EArmorSlot::Head:  TargetMesh = HeadMesh; break;
    case EArmorSlot::Chest: TargetMesh = ChestMesh; break;
    case EArmorSlot::Hands: TargetMesh = HandMesh; break;
    case EArmorSlot::Legs:  TargetMesh = LegMesh; break;
    case EArmorSlot::Feet:  TargetMesh = FootMesh; break;
    default: return;
    }

    if (TargetMesh)
    {
        TargetMesh->SetSkeletalMesh(NewArmor.ArmorMesh);
        if (bEnableArmorStats)
        {
            if (EquippedArmorWeights.Contains(Slot)) CurrentTotalWeight -= EquippedArmorWeights[Slot];
            EquippedArmorWeights.Add(Slot, NewArmor.WeightKg);
            CurrentTotalWeight += NewArmor.WeightKg;
            UpdateMovementSpeedBasedOnWeight();
        }
    }
}

void ABaseCharacter::UpdateMovementSpeedBasedOnWeight()
{
    if (!GetCharacterMovement()) return;
    if (!bEnableArmorStats) { GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed; return; }

    float WeightPenalty = CurrentTotalWeight * 5.0f;
    GetCharacterMovement()->MaxWalkSpeed = FMath::Clamp(DefaultWalkSpeed - WeightPenalty, 150.0f, DefaultWalkSpeed);
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