// DropItem.cpp
#include "DropItem.h"
#include "Components/SceneComponent.h"
#include "PlayerCharacter.h" 

DEFINE_LOG_CATEGORY(LogDropItem);

ADropItem::ADropItem()
{
	PrimaryActorTick.bCanEverTick = false;

	// [변경] 빈 SceneComponent를 루트로 설정
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void ADropItem::BeginPlay()
{
	Super::BeginPlay();
}

void ADropItem::Interact(APlayerCharacter* PlayerCharacter)
{
	if (!PlayerCharacter) return;

	if (OnPickup(PlayerCharacter))
	{
		DROP_LOG(Log, TEXT("Interaction Success: Picked up by Player %s"), *PlayerCharacter->GetName());
		Destroy();
	}
}

bool ADropItem::OnPickup(APlayerCharacter* PlayerCharacter)
{
	return false;
}