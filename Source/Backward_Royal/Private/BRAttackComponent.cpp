#include "BRAttackComponent.h"
#include "BaseCharacter.h"
#include "BaseWeapon.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogAttackComp);

UBRAttackComponent::UBRAttackComponent()
{
	PrimaryActorTick.bCanEverTick = false;
}

void UBRAttackComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소유자가 ABaseCharacter라면 메시 충돌 이벤트를 이 컴포넌트에 바인딩
	if (ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner()))
	{
		if (OwnerChar->GetMesh())
		{
			OwnerChar->GetMesh()->SetNotifyRigidBodyCollision(true);
			OwnerChar->GetMesh()->OnComponentHit.AddDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
		}
	}
}

void UBRAttackComponent::SetAttackDetection(bool bEnabled)
{
	bIsDetectionActive = bEnabled;
	if (!bEnabled) HitActors.Empty();
}

void UBRAttackComponent::InternalHandleOwnerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bIsDetectionActive || !OtherActor || OtherActor == GetOwner()) return;

	ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->HasAuthority()) return;

	float DamageValue = 10.0f; // 기본 주먹 데미지

	// 무기 장착 여부에 따른 데미지 결정
	if (OwnerChar->CurrentWeapon)
	{
		DamageValue = OwnerChar->CurrentWeapon->CurrentWeaponData.BaseDamage;
	}

	ProcessHitDamage(OtherActor, OtherComp, NormalImpulse, Hit, DamageValue);
}

void UBRAttackComponent::ProcessHitDamage(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit, float DamageMod)
{
	if (HitActors.Contains(OtherActor)) return;

	float ImpactForce = NormalImpulse.Size();
	float CalculatedDamage = ImpactForce * DamageMod * 0.01f;
	if (CalculatedDamage < 1.0f) CalculatedDamage = DamageMod;

	UGameplayStatics::ApplyDamage(OtherActor, CalculatedDamage, GetOwner()->GetInstigatorController(), GetOwner(), nullptr);

	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		OtherComp->AddImpulseAtLocation(-Hit.ImpactNormal * ImpactForce, Hit.ImpactPoint);
	}

	HitActors.Add(OtherActor);
	ATK_LOG(Log, TEXT("데미지 적용 -> 대상: %s, 피해량: %.1f"), *OtherActor->GetName(), CalculatedDamage);
}