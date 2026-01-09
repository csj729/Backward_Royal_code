// DropArmor.cpp
#include "DropArmor.h"
#include "PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY(LogDropArmor);

ADropArmor::ADropArmor()
{
	ArmorData.DisplayName = TEXT("Unknown Armor");

	// 1. 스켈레탈 메시 컴포넌트 생성
	ArmorMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmorMeshComp"));
	ArmorMeshComp->SetupAttachment(RootComponent); // 부모의 SceneRoot에 부착

	// 2. 충돌 설정 (이전과 동일하게 적용)
	// Pawn은 무시(통과), Visibility(E키)와 바닥은 블록
	ArmorMeshComp->SetCollisionProfileName(TEXT("Custom"));
	ArmorMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ArmorMeshComp->SetCollisionResponseToAllChannels(ECR_Block);
	ArmorMeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ArmorMeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	ArmorMeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ADropArmor::BeginPlay()
{
	Super::BeginPlay();

	// 데이터에 설정된 메시가 있다면 적용
	if (ArmorData.ArmorMesh)
	{
		ArmorMeshComp->SetSkeletalMesh(ArmorData.ArmorMesh);
		ARMOR_LOG(Display, TEXT("Armor Mesh Set: %s"), *ArmorData.ArmorMesh->GetName());
	}

	// (선택) 물리 시뮬레이션 활성화
	if (ArmorMeshComp)
	{
		ArmorMeshComp->SetSimulatePhysics(true);
	}
}

bool ADropArmor::OnPickup(ABaseCharacter* Character)
{
	// 1. 유효성 검사
	if (!Character)
	{
		ARMOR_LOG(Warning, TEXT("OnPickup Failed: Character is NULL"));
		return false;
	}

	// 2. 데이터 유효성 검사
	if (!ArmorData.ArmorMesh)
	{
		ARMOR_LOG(Warning, TEXT("OnPickup Failed: ArmorMesh is NULL in ArmorData"));
		return false;
	}

	ARMOR_LOG(Log, TEXT("Picking up Armor: %s (Slot: %d) -> Equipping to %s"),
		*ArmorData.DisplayName.ToString(), (uint8)ArmorData.EquipSlot, *Character->GetName());

	// 3. 장착 요청
	// EquipArmor가 BaseCharacter로 이동했으므로, 캐스팅 없이 바로 호출 가능합니다.
	Character->EquipArmor(ArmorData.EquipSlot, ArmorData);

	// true 반환 시 DropItem::Interact에서 Destroy() 호출됨
	return true;
}