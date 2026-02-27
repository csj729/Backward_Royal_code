// BRAttackComponent.cpp
#include "BRAttackComponent.h"
#include "BaseCharacter.h"
#include "BaseWeapon.h"
#include "TimerManager.h"
#include "GlobalBalanceData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogAttackComp);

// 기본 펀치 데미지 전역 변수

float UBRAttackComponent::Global_BasePunchDamage = 10.0f;

UBRAttackComponent::UBRAttackComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true); // 수정
}

void UBRAttackComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UBRAttackComponent::ServerSetAttackDetection_Implementation(bool bEnabled)
{
    SetAttackDetection(bEnabled);
}

void UBRAttackComponent::SetAttackDetection(bool bEnabled)
{
    bIsDetectionActive = bEnabled;
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;

    if (!GetOwner()->HasAuthority())
    {
        ServerSetAttackDetection(bEnabled);
    }

    // 공격이 새로 시작될 때마다 피격 액터 목록을 초기화하여 여러 번 휘두를 때 정상 타격되도록 보정
    if (bEnabled)
    {
        HitActors.Empty();
    }

    // 1. 무기 공격 설정
    if (OwnerChar->CurrentWeapon)
    {
        UStaticMeshComponent* WeaponMesh = OwnerChar->CurrentWeapon->WeaponMesh;
        if (WeaponMesh)
        {
            if (bEnabled)
            {
                if (GetOwner()->HasAuthority())
                {
                    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                    WeaponMesh->SetCollisionResponseToAllChannels(ECR_Block);
                    WeaponMesh->IgnoreActorWhenMoving(OwnerChar, true);

                    TArray<AActor*> AttachedActors;
                    OwnerChar->GetAttachedActors(AttachedActors);
                    for (AActor* Attached : AttachedActors)
                    {
                        WeaponMesh->IgnoreActorWhenMoving(Attached, true);
                    }
                    WeaponMesh->SetNotifyRigidBodyCollision(true);
                }

                if (!WeaponMesh->OnComponentHit.IsAlreadyBound(this, &UBRAttackComponent::InternalHandleOwnerHit))
                {
                    WeaponMesh->OnComponentHit.AddDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                }
            }
            else
            {
                if (GetOwner()->HasAuthority())
                {
                    // 무기가 캐릭터에 붙어있는 상태라면 충돌 해제
                    if (OwnerChar->CurrentWeapon && OwnerChar->CurrentWeapon->GetAttachParentActor() == OwnerChar)
                    {
                        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                        WeaponMesh->SetNotifyRigidBodyCollision(false);
                    }
                }
                WeaponMesh->OnComponentHit.RemoveDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                HitActors.Empty();
            }
        }
    }
    // 2. 맨손 공격 설정
    else
    {
        USkeletalMeshComponent* BodyMesh = OwnerChar->GetMesh();
        if (BodyMesh)
        {
            if (bEnabled)
            {
                BodyMesh->SetCollisionObjectType(ECC_WorldDynamic);
                BodyMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                BodyMesh->SetNotifyRigidBodyCollision(true);
                if (!BodyMesh->OnComponentHit.IsAlreadyBound(this, &UBRAttackComponent::InternalHandleOwnerHit))
                {
                    BodyMesh->OnComponentHit.AddDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                }
            }
            else
            {
                BodyMesh->SetCollisionObjectType(ECC_Pawn);
                BodyMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
                BodyMesh->SetNotifyRigidBodyCollision(false);
                BodyMesh->OnComponentHit.RemoveDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                HitActors.Empty();
            }

        }
    }
}

// 히트 스탑 적용 함수
void UBRAttackComponent::ApplyHitStop(float Duration)
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (OwnerChar)
    {
        // 1. 캐릭터 시간을 0.01배속(거의 정지)으로 설정
        OwnerChar->CustomTimeDilation = 0.01f;

        // 2. 지정된 시간 후에 ResetHitStop 호출
        GetWorld()->GetTimerManager().SetTimer(HitStopTimerHandle, this, &UBRAttackComponent::ResetHitStop, Duration, false);
    }
}

// 멀티캐스트 구현: 서버에서 호출하면 모든 클라이언트가 ApplyHitStop 실행
void UBRAttackComponent::MulticastApplyHitStop_Implementation(float Duration)
{
    ApplyHitStop(Duration);
}

// 히트 스탑 해제 및 애니메이션 종료
void UBRAttackComponent::ResetHitStop()
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (OwnerChar)
    {
        // 1. 시간 속도 정상 복구
        OwnerChar->CustomTimeDilation = 1.0f;

        // 2. 공격 애니메이션 강제 종료 (Idle 복귀)
        OwnerChar->StopAnimMontage();
    }
}

void UBRAttackComponent::InternalHandleOwnerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!bIsDetectionActive || !OtherActor || OtherActor == GetOwner()) return;

    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;

    // 맨손일 때 손/팔 부위만 판정
    if (OwnerChar->CurrentWeapon == nullptr)
    {
        FName HitBone = Hit.MyBoneName;
        FString HitBoneStr = HitBone.ToString().ToLower();
        bool bIsHandHit = HitBoneStr.Contains(TEXT("hand")) || HitBoneStr.Contains(TEXT("lowerarm"));
        if (!bIsHandHit) return;
    }

    if (!GetOwner()->HasAuthority()) return;

    if (HitActors.Contains(OtherActor)) return;

    ProcessHitDamage(OtherActor, OtherComp, NormalImpulse, Hit);
}

void UBRAttackComponent::MulticastPlayHitSound_Implementation(USoundBase* SoundToPlay, FVector Location, float Volume)
{
    if (SoundToPlay)
    {
        UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, Location, Volume);
    }
}


void UBRAttackComponent::ProcessHitDamage(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit)
{
    // 1. 안전 장치 (튕김 방지)
    AActor* OwnerActor = GetOwner();
    if (!IsValid(OwnerActor)) return;

    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(OwnerActor);
    if (!IsValid(OwnerChar)) return;

    ABaseWeapon* MyWeapon = OwnerChar->CurrentWeapon;

    if (HitActors.Contains(OtherActor)) return;

    // 2. 충격량 계산 (Clamp 적용)
    float ImpactForce = FMath::Clamp(NormalImpulse.Size(), 0.0f, 5000.0f);
    float ImpulseMultiplier = 1.0f;

    if (IsValid(MyWeapon))
    {
        ImpulseMultiplier = ABaseWeapon::GlobalImpulseMultiplier * MyWeapon->CurrentWeaponData.MassKg * MyWeapon->CurrentWeaponData.ImpulseCoefficient;
    }

    float FinalImpulsePower = FMath::Max(ImpactForce * ImpulseMultiplier, 500.0f);

    FVector ImpulseDir = -Hit.ImpactNormal;
    if (ImpulseDir.IsNearlyZero()) ImpulseDir = OwnerChar->GetActorForwardVector();

    FVector FinalImpulseVector = ImpulseDir * FinalImpulsePower;

    // 3. 충격량 정보 전달 (피격자에게)
    if (ABaseCharacter* Victim = Cast<ABaseCharacter>(OtherActor))
    {
        Victim->SetLastHitInfo(FinalImpulseVector, Hit.ImpactPoint);
    }

    // 4. 데미지 계산
    float CalculatedDamage = 0.0f;
    if (IsValid(MyWeapon))
    {
        CalculatedDamage = ImpactForce * MyWeapon->CurrentWeaponData.DamageCoefficient * MyWeapon->CurrentWeaponData.MassKg * ABaseWeapon::GlobalDamageMultiplier * 0.001f;
    }
    else
    {
        // [안전 처리] Global_BasePunchDamage 변수가 없다면 10.0f 기본값 사용
        float BaseDamage = 10.0f; 
        // 만약 헤더에 Global_BasePunchDamage가 있다면 위 줄을 지우고 사용하세요.
        CalculatedDamage = (ImpactForce * 0.001f) + BaseDamage;
    }

    // 디버그 출력
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("Hit: %s | Dmg: %.1f"), *OtherActor->GetName(), CalculatedDamage);
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, DebugMsg);
    }

    // 5. 유효타 처리 (데미지 적용 및 소리 재생)
    if (CalculatedDamage >= 3.0f)
    {
        UGameplayStatics::ApplyDamage(OtherActor, CalculatedDamage, OwnerChar->GetController(), OwnerChar, nullptr);
        
        if (IsValid(MyWeapon))
        {
            // 무기 내구도 감소
            MyWeapon->DecreaseDurability(CalculatedDamage);

            // [신규] 무기 타격음 재생 (멀티캐스트)
            if (MyWeapon->CurrentWeaponData.HitSound)
            {
                // 모든 클라이언트에게 소리 재생 명령
                MulticastPlayHitSound(MyWeapon->CurrentWeaponData.HitSound, Hit.ImpactPoint, 1.0f);
            }
        }
        else 
        {
            // [신규] 맨손 타격음 재생 (멀티캐스트)
            if (IsValid(OwnerChar) && OwnerChar->PunchHitSound)
            {
                MulticastPlayHitSound(OwnerChar->PunchHitSound, Hit.ImpactPoint, OwnerChar->PunchVolume);
            }
        }
    }

    // 6. 히트 스탑 및 물리 반발력 적용
    MulticastApplyHitStop(0.1f);

    if (OwnerChar->HasAuthority())
    {
        if (ABaseCharacter* VictimChar = Cast<ABaseCharacter>(OtherActor))
        {
            if (USkeletalMeshComponent* VictimMesh = VictimChar->GetMesh())
            {
                FName HitBone = Hit.BoneName;
                if (HitBone == NAME_None)
                {
                    HitBone = VictimMesh->FindClosestBone(Hit.ImpactPoint);
                }
                VictimMesh->AddImpulseAtLocation(FinalImpulseVector, Hit.ImpactPoint, HitBone);
            }
        }
        else if (OtherComp && OtherComp->IsSimulatingPhysics())
        {
            OtherComp->AddImpulseAtLocation(FinalImpulseVector, Hit.ImpactPoint, Hit.BoneName);
        }
    }

    HitActors.Add(OtherActor);
}

float UBRAttackComponent::GetCalculatedAttackSpeed() const
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return 1.0f;

    float FinalSpeed = 1.0f;

    if (OwnerChar->CurrentWeapon)
    {
        float WeaponMass = OwnerChar->CurrentWeapon->CurrentWeaponData.MassKg;
        float MassRatio = (WeaponMass > 0.1f) ? (StandardMass / WeaponMass) : 1.0f;
        float WeaponSpeedCoeff = OwnerChar->CurrentWeapon->CurrentWeaponData.AttackSpeedCoefficient;
        FinalSpeed = MassRatio * WeaponSpeedCoeff * ABaseWeapon::GlobalAttackSpeedMultiplier;
    }
    else
    {
        FinalSpeed = ABaseWeapon::GlobalAttackSpeedMultiplier;
    }

    return FMath::Clamp(FinalSpeed, 0.5f, 1.5f);
}