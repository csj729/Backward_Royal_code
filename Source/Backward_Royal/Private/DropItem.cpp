// DropItem.cpp
#include "DropItem.h"
#include "Components/SceneComponent.h"
#include "BaseCharacter.h"

DEFINE_LOG_CATEGORY(LogDropItem);

ADropItem::ADropItem()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    bReplicates = true;
}

void ADropItem::BeginPlay()
{
    Super::BeginPlay();
}

// [인터페이스] 상호작용 실행
void ADropItem::Interact(ABaseCharacter* Character)
{
    if (!Character) return;

    // OnPickup이 성공(true)하면 아이템은 할 일을 다 했으므로 파괴됨
    if (OnPickup(Character))
    {
        DROP_LOG(Log, TEXT("Picked up by %s"), *Character->GetName());
        Destroy();
    }
}

// [인터페이스] 안내 문구
FText ADropItem::GetInteractionPrompt()
{
    // 예: "획득: 아이템이름"
    return FText::Format(NSLOCTEXT("Interaction", "PickupItem", "획득: {0}"), FText::FromString(GetName()));
}

bool ADropItem::OnPickup(ABaseCharacter* Character)
{
    // 기본형은 아무 일도 안 함
    return false;
}