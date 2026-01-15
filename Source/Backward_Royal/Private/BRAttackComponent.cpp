#include "BRAttackComponent.h"
#include "BaseCharacter.h"
#include "BaseWeapon.h"
#include "TimerManager.h"
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
	// 서버에서 실제 판정 변수를 켭니다.
	SetAttackDetection(bEnabled);
}

void UBRAttackComponent::SetAttackDetection(bool bEnabled)
{
	bIsDetectionActive = bEnabled;
	ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());

	if (OwnerChar && OwnerChar->CurrentWeapon)
	{
		UStaticMeshComponent* WeaponMesh = OwnerChar->CurrentWeapon->WeaponMesh;
		if (WeaponMesh)
		{
			if (bEnabled)
			{
				// [서버 필수] 충돌 설정 강제 활성화
				WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				WeaponMesh->SetCollisionResponseToAllChannels(ECR_Block); // 일단 모든 채널 Block으로 테스트
				WeaponMesh->SetNotifyRigidBodyCollision(true); // Simulation Generates Hit Events

				// 무기 메시에 직접 바인딩되어 있는지 확인
				if (!WeaponMesh->OnComponentHit.IsAlreadyBound(this, &UBRAttackComponent::InternalHandleOwnerHit))
				{
					WeaponMesh->OnComponentHit.AddDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
				}

				GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Server: Collision Enabled"));
			}
			else
			{
				WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				WeaponMesh->OnComponentHit.RemoveDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
				GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Server: Collision Disabled"));

				HitActors.Empty();
			}
		}
	}
}

void UBRAttackComponent::InternalHandleOwnerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 1. 기본 조건 체크 (공통)
	if (!bIsDetectionActive || !OtherActor || OtherActor == GetOwner()) return;

	FString NetMode = GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client");
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::White, FString::Printf(TEXT("[%s] Physics Hit: %s"), *NetMode, *OtherActor->GetName()));

	// 2. 캐릭터 참조 가져오기
	ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
	if (!OwnerChar) return;

	// 클라이언트와 서버 양쪽에서 즉시 멈춰서 타격감을 극대화합니다.
	OwnerChar->StopAnimMontage();

	// 3. 권한 체크 (데미지 처리는 서버에서만)
	if (!GetOwner()->HasAuthority()) return;

	// 4. [서버 전용] 데미지 및 물리 로직
	if (HitActors.Contains(OtherActor)) return;

	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Applying Damage on Server"));

	float DamageValue = 10.0f;

	if (OwnerChar->CurrentWeapon) DamageValue = OwnerChar->CurrentWeapon->CurrentWeaponData.BaseDamage;
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
		OtherComp->AddImpulseAtLocation(-Hit.ImpactNormal * ImpactForce * ABaseWeapon::GlobalImpulseMultiplier, Hit.ImpactPoint);
	}

	HitActors.Add(OtherActor);
	ATK_LOG(Log, TEXT("데미지 적용 -> 대상: %s, 피해량: %.1f"), *OtherActor->GetName(), CalculatedDamage);
}