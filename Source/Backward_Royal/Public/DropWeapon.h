// DropWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "DropItem.h"
#include "DropWeapon.generated.h"

class ABaseWeapon;

DECLARE_LOG_CATEGORY_EXTERN(LogDropWeapon, Log, All);

UCLASS()
class BACKWARD_ROYAL_API ADropWeapon : public ADropItem
{
	GENERATED_BODY()

public:
	ADropWeapon();

protected:
	virtual void BeginPlay() override;
	virtual bool OnPickup(class APlayerCharacter* PlayerCharacter) override;

#define WEAPON_DROP_LOG(Verbosity, Format, ...) UE_LOG(LogDropWeapon, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

public:
	// [신규] 무기는 스태틱 메시 사용
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* WeaponMeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	TSubclassOf<ABaseWeapon> WeaponClass;
};