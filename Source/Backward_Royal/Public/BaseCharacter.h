// BaseCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArmorTypes.h"
#include "BaseCharacter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBaseChar, Log, All);

#define CHAR_LOG(Verbosity, Format, ...) UE_LOG(LogBaseChar, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

class ABaseWeapon; // 전방 선언
class UBRAttackComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathDelegate);

UCLASS()
class BACKWARD_ROYAL_API ABaseCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABaseCharacter();

protected:
    virtual void BeginPlay() override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Die();

public:
    // --- Modular Armor Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Armor")
    USkeletalMeshComponent* HeadMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Armor")
    USkeletalMeshComponent* ChestMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Armor")
    USkeletalMeshComponent* HandMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Armor")
    USkeletalMeshComponent* LegMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Armor")
    USkeletalMeshComponent* FootMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    UBRAttackComponent* AttackComponent;

    // --- Stats ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float DefaultWalkSpeed;

    // --- 체력 시스템 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MaxHP = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_CurrentHP)
    float CurrentHP;

    UFUNCTION()
    void OnRep_CurrentHP();

    // 데미지 처리 오버라이드
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDeathDelegate OnDeath;

    bool bIsDead = false;

    // =================================================================
    // [확정] 무기 시스템 (BaseCharacter 소유)
    // =================================================================

    // 현재 장착 중인 무기
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    ABaseWeapon* CurrentWeapon;

    // 무기 장착 (무기 액터를 받아 처리)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EquipWeapon(ABaseWeapon* NewWeapon);

    // 현재 무기 버리기
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void DropCurrentWeapon();

    // --- Functions ---
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void EquipArmor(EArmorSlot Slot, const FArmorData& NewArmor);

    UFUNCTION(BlueprintCallable, Category = "Customization")
    void SetArmorColor(EArmorSlot Slot, FLinearColor Color);
};