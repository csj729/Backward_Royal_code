// DropArmor.cpp
#include "DropArmor.h"
#include "PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY(LogDropArmor);

ADropArmor::ADropArmor()
{
	ArmorData.RowName = TEXT("Unknown Armor");

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

bool ADropArmor::OnPickup(APlayerCharacter* PlayerCharacter)
{
	// 1. 유효성 검사
	if (!PlayerCharacter)
	{
		ARMOR_LOG(Warning, TEXT("OnPickup Failed: PlayerCharacter is NULL"));
		return false;
	}

	// 2. 데이터 유효성 검사 (데이터가 비어있으면 장착 불가)
	if (!ArmorData.ArmorMesh)
	{
		ARMOR_LOG(Warning, TEXT("OnPickup Failed: ArmorMesh is NULL in ArmorData"));
		return false;
	}

	ARMOR_LOG(Log, TEXT("Picking up Armor: %s (Slot: %d) -> Equipping to Player %s"),
		*ArmorData.RowName.ToString(), (uint8)ArmorData.EquipSlot, *PlayerCharacter->GetName());

	// ==========================================================================================
	// [핵심 로직] 캐릭터에게 방어구 장착 요청
	// BaseCharacter::EquipArmor 함수 내부에서 다음 일이 일어납니다:
	// 1. Slot(예: Chest)에 맞는 메쉬 컴포넌트(ChestMesh)를 찾음
	// 2. 그 컴포넌트의 SkeletalMesh를 ArmorData.ArmorMesh로 교체
	// 3. (이미 설정된 Leader Pose에 의해) 캐릭터 애니메이션을 따라 움직임
	// ==========================================================================================
	PlayerCharacter->EquipArmor(ArmorData.EquipSlot, ArmorData);

	// 3. true를 반환하면 부모 클래스(DropItem)의 Interact 함수에서 Destroy()를 호출하여
	//    바닥에 떨어진 이 아이템을 삭제합니다.
	return true;
}