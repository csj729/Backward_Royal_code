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
                //GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Red, TEXT("Unarmed Attack Window OPEN"));
            }
            else
            {
                // 공격 종료 시 이벤트 생성 끄기 (최적화)
                BodyMesh->SetNotifyRigidBodyCollision(false);

                BodyMesh->OnComponentHit.RemoveDynamic(this, &UBRAttackComponent::InternalHandleOwnerHit);
                HitActors.Empty();

                //GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Green, TEXT("Unarmed Attack Window CLOSED"));
            }
        }
    }
}

void UBRAttackComponent::InternalHandleOwnerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 1. 기본 조건 체크 (공통)
	if (!bIsDetectionActive || !OtherActor || OtherActor == GetOwner()) return;

	//FString NetMode = GetOwner()->HasAuthority() ? TEXT("Server") : TEXT("Client");
	//GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::White, FString::Printf(TEXT("[%s] Physics Hit: %s"), *NetMode, *OtherActor->GetName()));

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
        bool bIsHandHit = HitBoneStr.Contains(TEXT("hand")) || HitBoneStr.Contains(TEXT("lowerarm"));
        if (!bIsHandHit)
        {
            // 몸싸움 중이라 몸통이나 발이 닿은 경우 -> 데미지 판정 X
            return;
        }
    }

	// 3. 권한 체크 (데미지 처리는 서버에서만)
	if (!GetOwner()->HasAuthority()) return;

	// 4. [서버 전용] 데미지 및 물리 로직
	if (HitActors.Contains(OtherActor)) return;

	//GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("Applying Damage on Server"));

    ProcessHitDamage(OtherActor, OtherComp, NormalImpulse, Hit);
}

void UBRAttackComponent::ProcessHitDamage(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit)
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    ABaseWeapon* MyWeapon = OwnerChar->CurrentWeapon;

	if (HitActors.Contains(OtherActor)) return;

    // 데미지 계산
	float ImpactForce = NormalImpulse.Size();
    float CalculatedDamage = 0.0f; 

    if (MyWeapon)
    {
        CalculatedDamage = ImpactForce * MyWeapon->CurrentWeaponData.DamageCoefficient * MyWeapon->CurrentWeaponData.MassKg * ABaseWeapon::GlobalDamageMultiplier * 0.001f;
    }
    else
    {
        CalculatedDamage = ImpactForce * 0.001f;
    }
	UGameplayStatics::ApplyDamage(OtherActor, CalculatedDamage, GetOwner()->GetInstigatorController(), GetOwner(), nullptr);

    if (OwnerChar && OwnerChar->CurrentWeapon)
    {
        // 무기가 있을 때만 내구도를 감소시킴
        MyWeapon->DecreaseDurability(CalculatedDamage);
    }

    ////

    // 충격량 계산
	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
        if(MyWeapon)
		    OtherComp->AddImpulseAtLocation(-Hit.ImpactNormal * ImpactForce * ABaseWeapon::GlobalImpulseMultiplier * MyWeapon->CurrentWeaponData.MassKg * MyWeapon->CurrentWeaponData.ImpulseCoefficient, Hit.ImpactPoint);
        else
            OtherComp->AddImpulseAtLocation(-Hit.ImpactNormal * ImpactForce, Hit.ImpactPoint);
	}
    ////
	HitActors.Add(OtherActor);

    GEngine->AddOnScreenDebugMessage(
        -1,
        2.f,
        FColor::Green,
        FString::Printf(TEXT("데미지 적용 -> 대상: %s, 피해량: %.1f"), *OtherActor->GetName(), CalculatedDamage)
    );
}

float UBRAttackComponent::GetCalculatedAttackSpeed() const
{
    ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner());
    if (!OwnerChar) return 1.0f;

    float FinalSpeed = 1.0f;

    // 1. 무기가 있는 경우: (표준무게 / 현재무기무게) * 글로벌 배율
    if (OwnerChar->CurrentWeapon)
    {
        float WeaponMass = OwnerChar->CurrentWeapon->CurrentWeaponData.MassKg;

        // 0으로 나누기 방지
        float MassRatio = (WeaponMass > 0.1f) ? (StandardMass / WeaponMass) : 1.0f;

        // 무기 데이터의 속도 계수(Coefficient)가 있다면 여기서 곱해줍니다.
        float WeaponSpeedCoeff = OwnerChar->CurrentWeapon->CurrentWeaponData.AttackSpeedCoefficient;

        FinalSpeed = MassRatio * WeaponSpeedCoeff * ABaseWeapon::GlobalAttackSpeedMultiplier;

        //GEngine->AddOnScreenDebugMessage(
        //    -1,
        //    2.f,
        //    FColor::Green,
        //    FString::Printf(TEXT("Weapon Attack Speed: %.2f (Mass: %.1f, GlobalMult: %.1f)"), FinalSpeed, WeaponMass, ABaseWeapon::GlobalAttackSpeedMultiplier)
        //);
    }
    // 2. 맨손인 경우: 글로벌 배율만 적용 (혹은 별도 맨손 계수)
    else
    {
        FinalSpeed = ABaseWeapon::GlobalAttackSpeedMultiplier;
        //GEngine->AddOnScreenDebugMessage(
        //    -1,
        //    2.f,
        //    FColor::Green,
        //    FString::Printf(TEXT("Unarmed Attack Speed: %.2f"), FinalSpeed)
        //);
    }

    // 게임플레이 한계치 설정 (너무 느리거나 빠르면 비현실적)
    return FMath::Clamp(FinalSpeed, 0.33f, 3.0f);
}