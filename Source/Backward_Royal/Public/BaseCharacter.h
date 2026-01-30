// BaseCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArmorTypes.h"
#include "BaseCharacter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBaseChar, Log, All);
#define CHAR_LOG(Verbosity, Format, ...) UE_LOG(LogBaseChar, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

class ABaseWeapon;
class UBRAttackComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, float, CurrentHP, float, MaxHP);

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

    // 사망 시 충격량(Impulse)과 위치(HitLocation)를 함께 전송
    UFUNCTION(NetMulticast, Reliable)
    void MulticastDie(FVector Impulse, FVector HitLocation, FVector ServerDieLocation, FRotator ServerDieRotation);

public:
    // [변경] 단순히 사망 시 사용할 충격량을 저장만 하는 함수 (즉시 적용 X)
    void SetLastHitInfo(FVector Impulse, FVector HitLocation);

    // [유지] 사망 시 적용할 마지막 충격량 저장 (서버 전용)
    FVector LastDeathImpulse = FVector::ZeroVector;
    FVector LastDeathHitLocation = FVector::ZeroVector;

    // --- Components ---
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

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnHPChanged OnHPChanged;

    void UpdateHPUI();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MaxHP = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_CurrentHP)
    float CurrentHP;

    UFUNCTION()
    void OnRep_CurrentHP();

    // --- Combat ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    UAnimMontage* OneHandedAttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    UAnimMontage* TwoHandedAttackMontage;

    UPROPERTY(EditAnywhere, Category = "Combat")
    UAnimMontage* PunchMontage_L;

    UPROPERTY(EditAnywhere, Category = "Combat")
    UAnimMontage* PunchMontage_R;

    bool bNextAttackIsLeft = false;

    void RequestAttack();
    void HandleWeaponBroken();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayWeaponAttack(UAnimMontage* MontageToPlay, APawn* RequestingPawn);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayPunch(UAnimMontage* TargetMontage);

    UFUNCTION(BlueprintCallable)
    void EnhancePhysics(bool bEnable);

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDeathDelegate OnDeath;

    bool bIsDead = false;

    // --- Weapon ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", Replicated)
    ABaseWeapon* CurrentWeapon;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EquipWeapon(ABaseWeapon* NewWeapon);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void DropCurrentWeapon();

    // --- Customization ---
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void EquipArmor(EArmorSlot Slot, const FArmorData& NewArmor);

    UFUNCTION(BlueprintCallable, Category = "Customization")
    void SetArmorColor(EArmorSlot Slot, FLinearColor Color);

protected:
    bool bIsCharacterAttacking = false;
};