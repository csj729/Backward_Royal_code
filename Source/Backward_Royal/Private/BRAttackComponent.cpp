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
    if (!OwnerChar) return;

    // 1. 무기가 있는 경우 (기존 로직)
    if (OwnerChar->CurrentWeapon)
    {
        UStaticMeshComponent* WeaponMesh = OwnerChar->CurrentWeapon->WeaponMesh;
        if (WeaponMesh)
        {
            if (bEnabled)
            {
                WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                WeaponMesh->SetCollisionResponseToAllChannels(ECR_Block);
                WeaponMesh->SetNotifyRigidBodyCollision(true);

                if (!WeaponMesh->OnComponentHit.IsAlreadyBound(this, &UBRAttackComponent::InternalHandleOwnerHit))
                {
                    WeaponMesh->OnComponentHit.AddDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                }
            }
            else
            {
                WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                WeaponMesh->OnComponentHit.RemoveDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                HitActors.Empty();
            }
        }
    }
    // 2. [추가] 무기가 없는 경우 (맨손 공격)
    else
    {
        USkeletalMeshComponent* BodyMesh = OwnerChar->GetMesh();

        if (BodyMesh)
        {
            if (bEnabled)
            {
                // [변경] 물리 속성(Block)은 건드리지 않고, 히트 이벤트 생성만 켭니다.
                BodyMesh->SetNotifyRigidBodyCollision(true);

                if (!BodyMesh->OnComponentHit.IsAlreadyBound(this, &UBRAttackComponent::InternalHandleOwnerHit))
                {
                    BodyMesh->OnComponentHit.AddDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                }

                // 디버그
                GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Red, TEXT("Unarmed Attack Window OPEN"));
            }
            else
            {
                // 공격 종료 시 이벤트 생성 끄기 (최적화)
                BodyMesh->SetNotifyRigidBodyCollision(false);

                BodyMesh->OnComponentHit.RemoveDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                HitActors.Empty();

                GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Green, TEXT("Unarmed Attack Window CLOSED"));
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

    // [중요] 부위별 판정 (맨손일 때만)
    if (OwnerChar->CurrentWeapon == nullptr)
    {
        // Physics Asset에 설정된 Body(Bone) 이름 확인
        FName HitBone = Hit.MyBoneName;
        FString HitBoneStr = HitBone.ToString().ToLower();

        // "hand" 또는 "fist" 등이 포함된 뼈인지 확인
        bool bIsHandHit = HitBoneStr.Contains(TEXT("hand")) || HitBoneStr.Contains(TEXT("fist")) || HitBoneStr.Contains(TEXT("index"));

        if (!bIsHandHit)
        {
            // 몸싸움 중이라 몸통이나 발이 닿은 경우 -> 데미지 판정 X
            return;
        }
    }

	// 클라이언트와 서버 양쪽에서 즉시 멈춰서 타격감을 극대화합니다.
	OwnerChar->StopAnimMontage();

	// 3. 권한 체크 (데미지 처리는 서버에서만)
	if (!GetOwner()->HasAuthority()) return;

	// 4. [서버 전용] 데미지 및 물리 로직
	if (HitActors.Contains(OtherActor)) return;

	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Applying Damage on Server"));

    float DamageValue = 0.0f;

    if (OwnerChar->CurrentWeapon)
    {
        DamageValue = OwnerChar->CurrentWeapon->CurrentWeaponData.BaseDamage;
    }
    else
    {
        // 맨손 기본 데미지 설정
        DamageValue = 10.0f;
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
		OtherComp->AddImpulseAtLocation(-Hit.ImpactNormal * ImpactForce * ABaseWeapon::GlobalImpulseMultiplier, Hit.ImpactPoint);
	}

	HitActors.Add(OtherActor);
	ATK_LOG(Log, TEXT("데미지 적용 -> 대상: %s, 피해량: %.1f"), *OtherActor->GetName(), CalculatedDamage);
}