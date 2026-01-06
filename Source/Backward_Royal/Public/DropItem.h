// DropItem.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableInterface.h"
#include "DropItem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDropItem, Log, All);

class ABaseCharacter;

UCLASS()
class BACKWARD_ROYAL_API ADropItem : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ADropItem();

protected:
	virtual void BeginPlay() override;

#define DROP_LOG(Verbosity, Format, ...) UE_LOG(LogDropItem, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* SceneRoot;

	// --- Interface Implementation ---
	virtual void Interact(class ABaseCharacter* Character) override;
	virtual FText GetInteractionPrompt() override;

protected:
	// 자식 클래스(DropArmor)에서 실제 데이터 획득 로직 구현
	virtual bool OnPickup(ABaseCharacter* Character);
};