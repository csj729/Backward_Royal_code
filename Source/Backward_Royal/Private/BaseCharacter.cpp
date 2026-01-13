// BaseCharacter.cpp
#include "BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "BRAttackComponent.h"
#include "Net/UnrealNetwork.h"
#include "BaseWeapon.h"

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

    AttackComponent = CreateDefaultSubobject<UBRAttackComponent>(TEXT("AttackComponent"));

    DefaultWalkSpeed = 600.0f;
    CurrentWeapon = nullptr; // 무기 초기화

    CurrentHP = MaxHP;
    bReplicates = true;
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

void ABaseCharacter::EquipWeapon(ABaseWeapon* NewWeapon)
{
    if (!NewWeapon) return;

    // 기존 무기 제거
    if (CurrentWeapon)
    {
        DropCurrentWeapon();
    }

    CurrentWeapon = NewWeapon;
    CurrentWeapon->SetOwner(this);
    CurrentWeapon->OnEquipped();

    // -------------------------------------------------------
    // [장착 로직] 무기(Grip) <-> 캐릭터(RightHandSocket) 일치시키기
    // -------------------------------------------------------
    FName CharacterSocketName = TEXT("RightHandSocket");

    // 무기 소켓 이름 (무기 BP에서 설정 가능, 기본값 "Grip")
    FName WeaponGripSocketName = NewWeapon->GripSocketName;
    if (WeaponGripSocketName.IsNone())
    {
        WeaponGripSocketName = TEXT("Grip");
    }

    USkeletalMeshComponent* AttachTarget = GetMesh();

    // 1. 일단 캐릭터의 손 소켓에 무기를 부착 (이 시점엔 무기의 원점이 손에 붙음)
    if (AttachTarget->DoesSocketExist(CharacterSocketName))
    {
        CurrentWeapon->AttachToComponent(AttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale, CharacterSocketName);
    }
    else
    {
        // 소켓이 없으면 안전하게 hand_r에 부착
        CurrentWeapon->AttachToComponent(AttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
        CHAR_LOG(Warning, TEXT("Socket '%s' missing. Attached to 'hand_r'."), *CharacterSocketName.ToString());
    }

    // 2. 무기 메쉬에 'Grip' 소켓이 있다면 위치 보정 수행
    if (CurrentWeapon->WeaponMesh && CurrentWeapon->WeaponMesh->DoesSocketExist(WeaponGripSocketName))
    {
        // (1) 무기 원점 기준, Grip 소켓의 상대 위치(Transform)를 가져옴
        FTransform GripTransform = CurrentWeapon->WeaponMesh->GetSocketTransform(WeaponGripSocketName, RTS_Component);

        // (2) 그 위치의 역(Inverse)을 무기의 상대 Transform으로 설정
        // 원리: Grip이 (10,0,0)에 있다면 무기를 (-10,0,0)으로 옮겨야 Grip이 (0,0,0)인 손 위치에 오게 됨
        CurrentWeapon->SetActorRelativeTransform(GripTransform.Inverse());

        CHAR_LOG(Log, TEXT("Adjusted weapon position using socket '%s'"), *WeaponGripSocketName.ToString());
    }
    else
    {
        CHAR_LOG(Warning, TEXT("Weapon socket '%s' not found. Weapon attached at Pivot."), *WeaponGripSocketName.ToString());
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

void ABaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABaseCharacter, CurrentHP);
}

float ABaseCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDead) return 0.0f;

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    CurrentHP = FMath::Clamp(CurrentHP - ActualDamage, 0.0f, MaxHP);

    if (CurrentHP <= 0.0f) Die();

    return ActualDamage;
}

void ABaseCharacter::Die()
{
    if (bIsDead) return;
    bIsDead = true;

    CHAR_LOG(Warning, TEXT("Character Died."));

    // 충돌 및 물리 설정
    GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetMesh()->SetSimulatePhysics(true); // Ragdoll 효과
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));

    OnDeath.Broadcast();
}

void ABaseCharacter::OnRep_CurrentHP()
{
    // 이 함수는 서버에서 CurrentHP 변수가 변경되어 클라이언트로 복제될 때 실행됩니다.
    // 보통 여기에서 체력 바(UI)를 업데이트하는 로직을 넣습니다.

    if (CurrentHP <= 0.0f)
    {
        // 사망 처리 등 클라이언트 측 가시적 효과가 필요하다면 여기서 호출 가능
        // Die(); 
    }

    CHAR_LOG(Log, TEXT("HP가 복제되었습니다. 현재 HP: %.1f"), CurrentHP);
}