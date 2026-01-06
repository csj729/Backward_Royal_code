// DropArmor.h
#pragma once

#include "CoreMinimal.h"
#include "DropItem.h"
#include "ArmorTypes.h"
#include "DropArmor.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDropArmor, Log, All);

UCLASS()
class BACKWARD_ROYAL_API ADropArmor : public ADropItem
{
	GENERATED_BODY()

public:
	ADropArmor();

protected:
	virtual void BeginPlay() override;
	virtual bool OnPickup(class APlayerCharacter* PlayerCharacter) override;

#define ARMOR_LOG(Verbosity, Format, ...) UE_LOG(LogDropArmor, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

public:
	// [신규] 방어구는 스켈레탈 메시를 사용하므로 여기에 선언
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* ArmorMeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	FArmorData ArmorData;
};