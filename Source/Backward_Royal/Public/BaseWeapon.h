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

    // --- 밸런싱 배율 (개별 무기용) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Balancing")
    float DamageCoefficient;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Balancing")
    float ImpulseCoefficient;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Balancing")
    float AttackSpeedCoefficient;

    // --- 전역 밸런싱 배율 (모든 무기 공통) ---
    // static으로 선언하여 모든 무기 인스턴스가 하나의 값을 공유하게 합니다.
    static float GlobalDamageMultiplier;
    static float GlobalImpulseMultiplier;
    static float GlobalAttackSpeedMultiplier;

    // 최종 계산된 수치를 가져오는 헬퍼 함수
    UFUNCTION(BlueprintCallable, Category = "Weapon|Stats")
    float GetFinalDamage() const { return CurrentWeaponData.BaseDamage * DamageCoefficient * GlobalDamageMultiplier; }

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