// DropWeapon.cpp
#include "DropWeapon.h"
#include "PlayerCharacter.h"
#include "BaseWeapon.h"
#include "Components/StaticMeshComponent.h"

DEFINE_LOG_CATEGORY(LogDropWeapon);

ADropWeapon::ADropWeapon()
{
	// 1. 스태틱 메시 컴포넌트 생성
	WeaponMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComp"));
	WeaponMeshComp->SetupAttachment(RootComponent);

	// 2. 충돌 설정 (DropArmor와 동일)
	WeaponMeshComp->SetCollisionProfileName(TEXT("Custom"));
	WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WeaponMeshComp->SetCollisionResponseToAllChannels(ECR_Block);
	WeaponMeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	WeaponMeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	WeaponMeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ADropWeapon::BeginPlay()
{
	Super::BeginPlay();

	// WeaponClass의 CDO(Class Default Object)에서 메시를 가져와서 보여줄 수도 있습니다.
	// 하지만 간단하게는 블루프린트에서 직접 Mesh를 지정해주는 것이 편합니다.

	if (WeaponClass)
	{
		WEAPON_DROP_LOG(Display, TEXT("Weapon Drop Spawned. Contains: %s"), *WeaponClass->GetName());
	}
}

bool ADropWeapon::OnPickup(APlayerCharacter* PlayerCharacter)
{
	if (!PlayerCharacter) return false;
	if (!WeaponClass) return false;

	WEAPON_DROP_LOG(Log, TEXT("Giving Weapon %s to Player %s"), *WeaponClass->GetName(), *PlayerCharacter->GetName());

	// [Todo] EquipWeapon 함수 호출
	// PlayerCharacter->EquipWeapon(WeaponClass);

	return true;
}