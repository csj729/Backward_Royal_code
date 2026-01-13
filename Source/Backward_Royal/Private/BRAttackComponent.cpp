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

void UBRAttackComponent::ServerSetAttackDetection_Implementation(bool bEnabled)
{
	// 서버에서 실제 판정 변수를 켭니다.
	SetAttackDetection(bEnabled);
}

void UBRAttackComponent::SetAttackDetection(bool bEnabled)
{
	bIsDetectionActive = bEnabled;

	// 서버라면 무기 메쉬의 물리 엔진 설정을 직접 건드려야 함
	ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
	if (OwnerChar && OwnerChar->HasAuthority() && OwnerChar->CurrentWeapon)
	{
		UStaticMeshComponent* WeaponMesh = OwnerChar->CurrentWeapon->WeaponMesh;
		if (WeaponMesh)
		{
			if (bEnabled)
			{
				// Hit 이벤트를 발생시키기 위한 필수 3종 세트
				WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				WeaponMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				WeaponMesh->SetNotifyRigidBodyCollision(true); // "Simulation Generates Hit Events"
			}
			else
			{
				WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}

	if (!bEnabled) HitActors.Empty();
}

void UBRAttackComponent::InternalHandleOwnerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 1. 함수 진입 확인
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("1. Hit Entered"));

	// 2. 기본 조건 체크 (자기 자신 제외 및 판정 활성화 여부)
	if (!bIsDetectionActive) {
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Exit: Detection Not Active"));
		return;
	}
	if (!OtherActor || OtherActor == GetOwner()) return;

	// 3. 중복 타격 방지 (이미 이 액터를 때렸다면 리턴)
	if (HitActors.Contains(OtherActor)) {
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("Exit: Already Hit This Actor"));
		return;
	}

	// 4. 권한 체크 확인
	ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
	if (!OwnerChar) return;

	if (!OwnerChar->HasAuthority()) {
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Orange, TEXT("Exit: No Authority (Client)"));
		// 멀티플레이어라면 여기서 막히는 게 맞습니다. 데미지는 서버에서만 주니까요.
		// 하지만 애니메이션 중단은 클라이언트에서도 보고 싶다면 이 아래로 로직을 옮겨야 합니다.
		return;
	}

	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("2. Passed All Checks - Applying Damage"));

	// 5. 데미지 처리
	float DamageValue = 10.0f;
	if (OwnerChar->CurrentWeapon)
	{
		DamageValue = OwnerChar->CurrentWeapon->CurrentWeaponData.BaseDamage;
	}

	// ProcessHitDamage 내부에서 HitActors.Add(OtherActor)를 반드시 호출해야 합니다!
	ProcessHitDamage(OtherActor, OtherComp, NormalImpulse, Hit, DamageValue);

	// 6. 0.1초 뒤 애니메이션 중단 로직
	UWorld* World = GetWorld();
	if (World)
	{
		FTimerHandle StopTimerHandle;
		TWeakObjectPtr<ABaseCharacter> WeakOwner = OwnerChar;
		World->GetTimerManager().SetTimer(StopTimerHandle, FTimerDelegate::CreateLambda([WeakOwner]()
			{
				if (WeakOwner.IsValid())
				{
					WeakOwner->StopAnimMontage();
				}
			}), 0.1f, false);
	}
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