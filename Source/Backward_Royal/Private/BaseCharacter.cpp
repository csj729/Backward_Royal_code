// BaseCharacter.cpp
#include "BaseCharacter.h"
#include "BaseWeapon.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "BRAttackComponent.h"
#include "UpperBodyPawn.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "BRPlayerState.h"
#include "BRGameMode.h"
#include "Kismet/GameplayStatics.h"

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

    if (GetCapsuleComponent())
    {
        // Pawn 채널(다른 캐릭터)에 대해 Block 설정
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    }

    // 2. 메시 컴포넌트: 이동 충돌에서 제외 (Overlap) -> 지터링 원인 제거
    if (GetMesh())
    {
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        // 메시끼리는 절대 서로 밀어내지 않도록 Overlap으로 설정
        // 이렇게 해야 캡슐끼리만 부딪히고, 메시는 부드럽게 겹쳐서 지터링이 사라짐
        GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

        GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

        // 식별 태그 (공격 판정용으로 유지)
        GetMesh()->ComponentTags.Add(TEXT("CharacterMesh"));
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

    if (GetCharacterMovement())
    {
        DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
    }

    if (HasAuthority())
    {
        CurrentHP = MaxHP;
    }

    UpdateHPUI();
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
    if (bIsStunned || CurrentHP <= 0.0f || IsDead()) return;

    // 1. 무기를 들고 있는 경우
    if (CurrentWeapon)
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

        // 재생할 몽타주 선택 로직
        UAnimMontage* MontageToPlay = nullptr;

        // 무기 타입 확인 (BaseWeapon.h의 EWeaponType 사용)
        switch (CurrentWeapon->CurrentWeaponData.WeaponType)
        {
        case EWeaponType::OneHanded:
            MontageToPlay = OneHandedAttackMontage;
            break;
        case EWeaponType::TwoHanded:
            MontageToPlay = TwoHandedAttackMontage;
            break;
        default:
            // 예외 처리: 기본적으로 한손 모션 사용하거나 로그 출력
            MontageToPlay = OneHandedAttackMontage;
            CHAR_LOG(Warning, TEXT("Unknown Weapon Type. Defaulting to OneHanded."));
            break;
        }

        if (AnimInstance && MontageToPlay)
        {
            // 이미 해당 몽타주가 재생 중이면 패스
            if (AnimInstance->Montage_IsPlaying(MontageToPlay)) return;

            // [핵심] 선택된 몽타주를 인자로 전달 (RequestingPawn은 없으므로 nullptr)
            MulticastPlayWeaponAttack(MontageToPlay, nullptr);
        }
        return;
    }

    // 2. 맨손 공격 로직 (기존 유지)
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (!AnimInstance) return;

    if (AnimInstance->Montage_IsPlaying(PunchMontage_L) || AnimInstance->Montage_IsPlaying(PunchMontage_R)) return;

    UAnimMontage* SelectedMontage = bNextAttackIsLeft ? PunchMontage_L : PunchMontage_R;
    if (SelectedMontage)
    {
        MulticastPlayPunch(SelectedMontage);
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
    DOREPLIFETIME(ABaseCharacter, LastDeathInfo);
    DOREPLIFETIME(ABaseCharacter, bIsStunned);
}

bool ABaseCharacter::IsDead() const
{
    // BRPlayerState의 EPlayerStatus를 확인
    if (ABRPlayerState* PS = GetPlayerState<ABRPlayerState>())
    {
        return PS->CurrentStatus == EPlayerStatus::Dead;
    }
    return false;
}

void ABaseCharacter::SetLastHitInfo(FVector Impulse, FVector HitLocation)
{
    LastDeathInfo.Impulse = Impulse;
    LastDeathInfo.HitLocation = HitLocation;
}

float ABaseCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 이미 사망 상태이거나 스턴 상태면 데미지 무시
    if (IsDead() || bIsStunned) return 0.0f;

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    CurrentHP = FMath::Clamp(CurrentHP - ActualDamage, 0.0f, MaxHP);

    UpdateHPUI();

    if (CurrentHP <= 0.0f)
    {
        bool bUseStun = false;
        if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
        {
            // 현재 게임모드가 스턴 규칙을 사용하는지 확인
            bUseStun = GM->bUseStunInsteadOfDeath;
        }

        if (bUseStun)
        {
            EnterStunState(); // 사망 대신 스턴 진입
        }
        else
        {
            // 기존 배틀로얄 데스 로직
            Die(LastDeathInfo.Impulse, LastDeathInfo.HitLocation);
        }
    }

    return ActualDamage;
}

void ABaseCharacter::Die(FVector KillImpulse, FVector HitLocation)
{
    // 중복 사망 방지
    if (IsDead()) return;

    // 1. PlayerState 상태 변경 (Alive -> Dead)
    if (ABRPlayerState* PS = GetPlayerState<ABRPlayerState>())
    {
        PS->SetPlayerStatus(EPlayerStatus::Dead);
    }

    // 2. 사망 정보 확정 저장 (서버 위치 등)
    LastDeathInfo.Impulse = KillImpulse;
    LastDeathInfo.HitLocation = HitLocation;
    LastDeathInfo.ServerDieLocation = GetActorLocation();
    LastDeathInfo.ServerDieRotation = GetActorRotation();

    // 3. 래그돌/이펙트 처리
    PerformDeathVisuals();

    // 4. 게임 모드에 알림
    if (ABRGameMode* GM = GetWorld()->GetAuthGameMode<ABRGameMode>())
    {
        GM->OnPlayerDied(this);
    }
}

void ABaseCharacter::PerformDeathVisuals_Implementation()
{
    // 이미 물리 시뮬레이션 중이라면 중복 실행 방지
    if (GetMesh()->IsSimulatingPhysics()) return;

    // --- 변수 가져오기 ---
    FVector Impulse = LastDeathInfo.Impulse;
    FVector HitLocation = LastDeathInfo.HitLocation;
    FVector ServerLoc = LastDeathInfo.ServerDieLocation;
    FRotator ServerRot = LastDeathInfo.ServerDieRotation;

    CHAR_LOG(Warning, TEXT("PerformDeathVisuals Executed - Impulse: %s"), *Impulse.ToString());

    // [중요] 이동 동기화 해제 (서버와 클라이언트 연결 고리 끊기)
    SetReplicateMovement(false);

    // ==========================================
    // [핵심] 하체 및 부착된 상체의 조작 완전 차단 (멀티캐스트로 각 클라이언트 로컬에서 실행)
    // ==========================================
    // 1. 하체(자신)의 조작 비활성화
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DisableInput(PC);
    }

    // 2. 자신에게 부착된 상체(UpperBody) 폰을 찾아 조작 및 애니메이션 비활성화
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);
    for (AActor* Actor : AttachedActors)
    {
        if (APawn* AttachedPawn = Cast<APawn>(Actor))
        {
            // 상체 폰의 입력을 제거하여 로컬(클라이언트)에서의 공격/시점 회전 완전 차단
            if (APlayerController* UpperPC = Cast<APlayerController>(AttachedPawn->GetController()))
            {
                AttachedPawn->DisableInput(UpperPC);
            }
            // 상체 폰의 Tick을 꺼서 회전 및 애니메이션 업데이트 완전 정지
            AttachedPawn->SetActorTickEnabled(false);
        }
    }
    // ==========================================

    // 1. 위치 싱크 (오차 보정)
    if (!HasAuthority() && FVector::DistSquared(GetActorLocation(), ServerLoc) < 250000.0f)
    {
        SetActorLocationAndRotation(ServerLoc, ServerRot, false, nullptr, ETeleportType::TeleportPhysics);
    }

    // 2. 캡슐 및 이동 컴포넌트 비활성화
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        GetCharacterMovement()->SetComponentTickEnabled(false);
    }

    // 피지컬 애니메이션 해제 (순수 랙돌 전환을 위해)
    UPhysicalAnimationComponent* PhysAnimComp = FindComponentByClass<UPhysicalAnimationComponent>();
    if (PhysAnimComp)
    {
        PhysAnimComp->SetSkeletalMeshComponent(nullptr);
    }

    // 3. 랙돌 활성화 및 힘 적용
    if (GetMesh())
    {
        GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        // 루트 본 하위 모든 바디 시뮬레이션
        FName RootBoneName = GetMesh()->GetBoneName(0);
        GetMesh()->SetAllBodiesBelowSimulatePhysics(RootBoneName, true, true);

        GetMesh()->SetSimulatePhysics(true);
        GetMesh()->WakeAllRigidBodies();

        // 4. 충격량 적용
        if (GetMesh()->IsSimulatingPhysics() && !Impulse.IsNearlyZero())
        {
            if (!HitLocation.IsNearlyZero())
            {
                FName ClosestBone = GetMesh()->FindClosestBone(HitLocation);
                if (ClosestBone != NAME_None)
                {
                    GetMesh()->AddImpulseAtLocation(Impulse, HitLocation, ClosestBone);
                }
                else
                {
                    GetMesh()->AddImpulseAtLocation(Impulse, HitLocation);
                }
            }
            else
            {
                GetMesh()->AddImpulse(Impulse);
            }
        }
    }
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

void ABaseCharacter::MulticastPlayWeaponAttack_Implementation(UAnimMontage* MontageToPlay, APawn* RequestingPawn)
{
    // 몽타주가 없으면 실행 불가
    if (!MontageToPlay) return;

    if (GetMesh())
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        if (AnimInstance)
        {
            float AttackSpeed = AttackComponent->GetCalculatedAttackSpeed();

            // [핵심] 인자로 받은 몽타주를 재생
            AnimInstance->Montage_Play(MontageToPlay, AttackSpeed);
            
            // [신규] 무기 휘두르는 소리 재생 (모든 클라이언트에서 실행됨)
            if (CurrentWeapon && CurrentWeapon->CurrentWeaponData.SwingSound)
            {
                UGameplayStatics::PlaySoundAtLocation(
                    this, 
                    CurrentWeapon->CurrentWeaponData.SwingSound, 
                    GetActorLocation()
                );
            }

            // [기존 로직 유지] UpperBodyPawn이 요청한 경우(VR 등), 몽타주 종료 콜백 연결
            if (AUpperBodyPawn* UpperPawn = Cast<AUpperBodyPawn>(RequestingPawn))
            {
                FOnMontageEnded EndDelegate;
                EndDelegate.BindUObject(UpperPawn, &AUpperBodyPawn::OnAttackMontageEnded);

                // 해당 몽타주가 끝날 때 델리게이트 호출
                AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
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
            // [신규] 주먹 휘두르는 소리 재생
            if (PunchSwingSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, PunchSwingSound, GetActorLocation());
            }

            float AttackSpeed = AttackComponent->GetCalculatedAttackSpeed();
            AnimInstance->Montage_Play(TargetMontage, AttackSpeed);
        }
    }
}

// 공격 시작 시 호출 (AnimNotify 등에서 활용)
void ABaseCharacter::EnhancePhysics(bool bEnable)
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

void ABaseCharacter::HandleWeaponBroken()
{
    CurrentWeapon = nullptr;
}

void ABaseCharacter::EnterStunState()
{
    if (bIsStunned || !HasAuthority()) return;

    bIsStunned = true;

    // 모든 클라이언트에 스턴 효과 및 조작 불가 적용
    MulticastEnterStunState();

    // StunDuration 초 후에 RecoverFromStun 함수 실행
    GetWorld()->GetTimerManager().SetTimer(StunTimerHandle, this, &ABaseCharacter::RecoverFromStun, StunDuration, false);

    CHAR_LOG(Log, TEXT("Player Entered Stun State"));
}

void ABaseCharacter::MulticastEnterStunState_Implementation()
{
    // 캐릭터 이동 완전 정지 및 비활성화
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
    }

    // 컨트롤러의 입력 비활성화 (로컬 조작 방지)
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DisableInput(PC);
    }

    // 블루프린트 이벤트 호출 (모든 클라이언트에서 실행됨)
    OnEnterStunState();
}

void ABaseCharacter::RecoverFromStun()
{
    if (!HasAuthority()) return;

    bIsStunned = false;

    // 체력을 최대로 회복시키고 UI 업데이트
    CurrentHP = MaxHP;
    UpdateHPUI();

    // 모든 클라이언트에 조작 복구 적용
    MulticastRecoverFromStun();

    CHAR_LOG(Log, TEXT("Player Recovered From Stun"));
}

void ABaseCharacter::MulticastRecoverFromStun_Implementation()
{
    // 캐릭터 이동 다시 활성화
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    // 컨트롤러 입력 다시 활성화
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        EnableInput(PC);
    }

    // 블루프린트 이벤트 호출 (모든 클라이언트에서 실행됨)
    OnRecoverFromStun();
}