// DropItem.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DropItem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDropItem, Log, All);

class APlayerCharacter;

UCLASS()
class BACKWARD_ROYAL_API ADropItem : public AActor
{
	GENERATED_BODY()

public:
	ADropItem();

protected:
	virtual void BeginPlay() override;

#define DROP_LOG(Verbosity, Format, ...) UE_LOG(LogDropItem, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

public:
	// [변경] ItemMesh 삭제 -> 자식 클래스가 각자 알맞은 컴포넌트를 가집니다.
	// 대신 위치 기준점이 될 Root가 필요합니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* SceneRoot;

	// 상호작용 함수
	void Interact(APlayerCharacter* PlayerCharacter);

protected:
	virtual bool OnPickup(APlayerCharacter* PlayerCharacter);
};