// BaseCharacter.cpp
#include "BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "BRAttackComponent.h"
#include "UpperBodyPawn.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
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

    // PhysicsControlComp = CreateDefaultSubobject<UPhysicsControlComponent>(TEXT("PhysicsControlComp"));

    if (GetMesh())
    {
        // 1. 물리(Physics)와 쿼리(Query) 모두 활성화
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        // 2. 기본 프로필 설정 (CharacterMesh 권장)
        GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));

        // 3. Pawn(다른 플레이어)에 대해 Block 설정 -> 서로 밀리게 됨
        GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

        // 4. 카메라는 무시 (카메라가 몸 뚫을 때 덜컹거림 방지)
        GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

        // 5. 평소에는 Hit Event를 꺼둬서 불필요한 연산 방지 (공격 때만 BRAttackComponent가 켬)
        GetMesh()->SetNotifyRigidBodyCollision(false);
    }

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

    //if (PhysicsControlComp && GetMesh())
    //{
    //    SetupArmPhysicsControls();
    //}

    if (GetCharacterMovement())
    {
        DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
    }
}

void ABaseCharacter::EquipWeapon(ABaseWeapon* NewWeapon)
{
    if (!NewWeapon) return;

    if (!HasAuthority()) return;

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

// [신규] 공격 요청 처리 함수
void ABaseCharacter::RequestAttack()
{
    // 1. 무기가 있는 경우: 기존 로직 유지
    if (CurrentWeapon)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance && AttackMontage)
        {
            if (AnimInstance->Montage_IsPlaying(AttackMontage)) return;
        }
        MulticastPlayWeaponAttack(nullptr);
        return;
    }

    // 2. 맨손 공격 로직 (번갈아 치기)
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (!AnimInstance) return;

    // [핵심] 현재 어떤 공격 몽타주라도 재생 중이면 입력을 무시
    if (AnimInstance->Montage_IsPlaying(PunchMontage_L) ||
        AnimInstance->Montage_IsPlaying(PunchMontage_R))
    {
        return;
    }

    // 재생할 손 결정
    UAnimMontage* SelectedMontage = bNextAttackIsLeft ? PunchMontage_L : PunchMontage_R;

    if (SelectedMontage)
    {
        MulticastPlayPunch(SelectedMontage);

        // [2025-11-18] 커스텀 디버그 로그 매크로 사용 (규칙 준수)
        CHAR_LOG(Log, TEXT("Starting Punch: %s"), bNextAttackIsLeft ? TEXT("Left") : TEXT("Right"));

        // 다음 손으로 변경
        bNextAttackIsLeft = !bNextAttackIsLeft;
    }
}

// 무기 버리기 구현
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
    DOREPLIFETIME(ABaseCharacter, CurrentWeapon);
}

float ABaseCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDead) return 0.0f;

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    CurrentHP = FMath::Clamp(CurrentHP - ActualDamage, 0.0f, MaxHP);

    UpdateHPUI();

    if (CurrentHP <= 0.0f) Die();

    return ActualDamage;
}

void ABaseCharacter::Die()
{
    // 이미 죽었으면 무시 (서버 기준)
    if (bIsDead) return;

    MulticastDie();
}

void ABaseCharacter::MulticastDie_Implementation()
{
    // 중복 실행 방지
    if (bIsDead) return;
    bIsDead = true;

    CHAR_LOG(Warning, TEXT("Character Died (Multicast)."));

    // 1. 캡슐 충돌 끄기 (시체끼리 길막 방지)
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 2. [중요] 이동 컴포넌트 비활성화
    // 이걸 안 끄면 "물리 엔진" vs "이동 컴포넌트"가 싸워서 캐릭터가 부들거리거나 이상하게 날아갑니다.
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        GetCharacterMovement()->SetComponentTickEnabled(false);
    }

    UPhysicalAnimationComponent* PhysAnimComp = FindComponentByClass<UPhysicalAnimationComponent>();
    if (PhysAnimComp)
    {
        // 1. 메쉬와의 연결을 끊거나
        PhysAnimComp->SetSkeletalMeshComponent(nullptr);

        CHAR_LOG(Log, TEXT("Physical Animation Disabled for Ragdoll."));
    }

    // 3. 메쉬 물리 시뮬레이션 (Ragdoll) 설정 수정
    if (GetMesh())
    {
        // 충돌 프로필과 활성화 설정
        GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        GetMesh()->SetSimulatePhysics(true);
    }

    // 4. 사망 이벤트 전파
    OnDeath.Broadcast();
}

void ABaseCharacter::UpdateHPUI()
{
    if (OnHPChanged.IsBound())
    {
        OnHPChanged.Broadcast(CurrentHP, MaxHP);
    }
}

void ABaseCharacter::OnRep_CurrentHP()
{
    // 이 함수는 서버에서 CurrentHP 변수가 변경되어 클라이언트로 복제될 때 실행됩니다.
    // 보통 여기에서 체력 바(UI)를 업데이트하는 로직을 넣습니다.
    UpdateHPUI();

    if (CurrentHP <= 0.0f)
    {
        // 사망 처리 등 클라이언트 측 가시적 효과가 필요하다면 여기서 호출 가능
        // Die(); 
    }

    CHAR_LOG(Log, TEXT("HP가 복제되었습니다. 현재 HP: %.1f"), CurrentHP);
}

void ABaseCharacter::MulticastPlayWeaponAttack_Implementation(APawn* RequestingPawn)
{
    if (AttackMontage && GetMesh())
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            float AttackSpeed = AttackComponent->GetCalculatedAttackSpeed();

            AnimInstance->Montage_Play(AttackMontage, AttackSpeed);

            // 전달받은 Pawn을 UpperBodyPawn으로 캐스팅
            if (AUpperBodyPawn* UpperPawn = Cast<AUpperBodyPawn>(RequestingPawn))
            {
                FOnMontageEnded EndDelegate;
              
                EndDelegate.BindUObject(UpperPawn, &AUpperBodyPawn::OnAttackMontageEnded);
                AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontage);
            }
        }
    }
}

void ABaseCharacter::MulticastPlayPunch_Implementation(UAnimMontage* TargetMontage)
{
    if (TargetMontage && GetMesh())
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            float AttackSpeed = AttackComponent->GetCalculatedAttackSpeed();
            AnimInstance->Montage_Play(TargetMontage, AttackSpeed);
        }
    }
}

// 공격 시작 시 호출 (AnimNotify 등에서 활용)
void ABaseCharacter::EnhanceFistPhysics(bool bEnable)
{
    // 팔 관련 본들의 이름을 배열로 관리하여 적용
    TArray<FName> RootArmBones = { TEXT("lowerarm_r"), TEXT("lowerarm_l") };
    for (const FName& BoneName : RootArmBones)
    {
        // 세 번째 인자인 bIncludeSelf를 true로 설정하여 upperarm 자체도 포함시킵니다.
        if(bEnable) GetMesh()->SetAllBodiesBelowSimulatePhysics(BoneName, false, true);
        else GetMesh()->SetAllBodiesBelowSimulatePhysics(BoneName, true, true);
    }
}