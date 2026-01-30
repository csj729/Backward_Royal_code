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

    // 1. 무기 공격
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
    // 2. 맨손 공격
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

void UBRAttackComponent::InternalHandleOwnerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!bIsDetectionActive || !OtherActor || OtherActor == GetOwner()) return;

    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return;

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

    // 최소 충격량 보장
    float FinalImpulsePower = FMath::Max(ImpactForce * ImpulseMultiplier, 500.0f);

    FVector ImpulseDir = -Hit.ImpactNormal;
    if (ImpulseDir.IsNearlyZero()) ImpulseDir = OwnerChar->GetActorForwardVector();

    FVector FinalImpulseVector = ImpulseDir * FinalImpulsePower;

    // [충격량 저장] 대상이 캐릭터라면 충격량을 미리 저장
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

    // [디버그 로그 추가] 화면에 데미지와 충격량 표시
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("Hit: %s | Damage: %.1f | Impulse: %.1f"),
            *OtherActor->GetName(), CalculatedDamage, FinalImpulsePower);

        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, DebugMsg);
    }

    if (CalculatedDamage >= 5.0f)
    {
        UGameplayStatics::ApplyDamage(OtherActor, CalculatedDamage, GetOwner()->GetInstigatorController(), GetOwner(), nullptr);

        if (MyWeapon)
        {
            MyWeapon->DecreaseDurability(CalculatedDamage);
        }
    }

    // [일반 물체 물리 적용]
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