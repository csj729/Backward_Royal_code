// BRAttackComponent.cpp
#include "BRAttackComponent.h"
#include "BaseCharacter.h"
#include "BaseWeapon.h"
#include "TimerManager.h"
#include "GlobalBalanceData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogAttackComp);

UBRAttackComponent::UBRAttackComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicated(true);
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
                BodyMesh->SetNotifyRigidBodyCollision(true);
                if (!BodyMesh->OnComponentHit.IsAlreadyBound(this, &UBRAttackComponent::InternalHandleOwnerHit))
                {
                    BodyMesh->OnComponentHit.AddDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                }
            }
            else
            {
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

        // 2. [요청 사항] 공격 애니메이션 강제 종료 (Idle 복귀)
        // 0.25초 정도의 블렌드 아웃 시간을 주어 부드럽게 돌아가도록 StopAnimMontage 사용
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

void UBRAttackComponent::ProcessHitDamage(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit)
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    ABaseWeapon* MyWeapon = OwnerChar->CurrentWeapon;

    if (HitActors.Contains(OtherActor)) return;

    // [충격량 계산]
    float ImpactForce = NormalImpulse.Size();
    float ImpulseMultiplier = 1.0f;

    if (MyWeapon)
    {
        ImpulseMultiplier = ABaseWeapon::GlobalImpulseMultiplier * MyWeapon->CurrentWeaponData.MassKg * MyWeapon->CurrentWeaponData.ImpulseCoefficient;
    }

    float FinalImpulsePower = FMath::Max(ImpactForce * ImpulseMultiplier, 500.0f);

    FVector ImpulseDir = -Hit.ImpactNormal;
    if (ImpulseDir.IsNearlyZero()) ImpulseDir = OwnerChar->GetActorForwardVector();

    FVector FinalImpulseVector = ImpulseDir * FinalImpulsePower;

    // [충격량 미리 주입]
    if (ABaseCharacter* Victim = Cast<ABaseCharacter>(OtherActor))
    {
        Victim->SetLastHitInfo(FinalImpulseVector, Hit.ImpactPoint);
    }

    // [데미지 계산]
    float CalculatedDamage = 0.0f;
    if (MyWeapon)
    {
        CalculatedDamage = ImpactForce * MyWeapon->CurrentWeaponData.DamageCoefficient * MyWeapon->CurrentWeaponData.MassKg * ABaseWeapon::GlobalDamageMultiplier * 0.001f;
    }
    else
    {
        CalculatedDamage = ImpactForce * 0.001f;
    }

    // 디버그 출력
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("Hit: %s | Dmg: %.1f | Impulse: %.0f"),
            *OtherActor->GetName(), CalculatedDamage, FinalImpulsePower);
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, DebugMsg);

        ATK_LOG(Log, TEXT("Target: %s, Damage: %.1f, Impulse: %.1f"), *OtherActor->GetName(), CalculatedDamage, FinalImpulsePower);
    }

    // 유효타 처리
    if (CalculatedDamage >= 3.0f)
    {
        UGameplayStatics::ApplyDamage(OtherActor, CalculatedDamage, GetOwner()->GetInstigatorController(), GetOwner(), nullptr);

        if (MyWeapon)
        {
            MyWeapon->DecreaseDurability(CalculatedDamage);
        }
    }

    // 공격 성공 시 히트 스탑 적용 (0.1초 멈춤 -> 이후 애니메이션 종료)
    MulticastApplyHitStop(0.1f);

    // 캐릭터 외 물체 물리 적용
    if (GetOwner()->HasAuthority() && OtherComp && OtherComp->IsSimulatingPhysics())
    {
        if (!Cast<ABaseCharacter>(OtherActor))
        {
            OtherComp->AddImpulseAtLocation(FinalImpulseVector, Hit.ImpactPoint);
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