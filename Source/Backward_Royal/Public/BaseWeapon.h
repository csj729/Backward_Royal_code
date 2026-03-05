// BaseWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableInterface.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
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

    // --- 전역 밸런싱 배율 (모든 무기 공통) ---
    static float GlobalDamageMultiplier;
    static float GlobalImpulseMultiplier;
    static float GlobalAttackSpeedMultiplier;
    static float GlobalDurabilityReduction;

    // --- 무기 데이터 및 메시 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UStaticMeshComponent* WeaponMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FName WeaponRowName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Weapon")
    FWeaponData CurrentWeaponData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FName GripSocketName;

    void LoadWeaponData();
    void InitializeWeaponStats(const FWeaponData& NewStats);

    virtual void OnEquipped();
    virtual void OnDropped();

    // 파괴 로직 함수
    void BreakWeapon();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_BreakWeaponVisual(const FTransform& SpawnTransform, class UGeometryCollection* InFracturedMesh);

    // DamageAmount: 공격 시 가한 데미지 (버전 2에서 사용)
    UFUNCTION(BlueprintCallable, Category = "Weapon|Durability")
    void DecreaseDurability(float DamageAmount);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float DurabilityReduction = 10.f;

    bool IsEquipped() { return bIsEquipped; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    bool bIsEquipped;

};