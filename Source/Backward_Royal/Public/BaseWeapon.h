// BaseWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableInterface.h"
#include "WeaponTypes.h"
#include "BaseWeapon.generated.h"

UCLASS()
class BACKWARD_ROYAL_API ABaseWeapon : public AActor, public IInteractableInterface
{
    GENERATED_BODY()

public:
    ABaseWeapon();

    // --- 인터페이스 구현 ---
    virtual void Interact(class ABaseCharacter* Character) override;
    virtual FText GetInteractionPrompt() override;

    // --- 무기 데이터 및 메시 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UStaticMeshComponent* WeaponMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UDataTable* MyDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FName WeaponRowName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    FWeaponData CurrentWeaponData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FName GripSocketName;

    void LoadWeaponData();
    void InitializeWeaponStats(const FWeaponData& NewStats);

    virtual void OnEquipped();
    virtual void OnDropped();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    bool bIsEquipped;
};