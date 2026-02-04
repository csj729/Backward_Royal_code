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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, float, CurrentHP, float, MaxHP);

// 사망 당시의 물리 정보를 저장할 구조체
USTRUCT(BlueprintType)
struct FDeathDamageInfo
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Impulse = FVector::ZeroVector;

    UPROPERTY()
    FVector HitLocation = FVector::ZeroVector;

    UPROPERTY()
    FVector ServerDieLocation = FVector::ZeroVector;

    UPROPERTY()
    FRotator ServerDieRotation = FRotator::ZeroRotator;
};

UCLASS()
class BACKWARD_ROYAL_API ABaseCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABaseCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
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
    
    // 서버에서 사망 함수 호출 시 충격량 정보도 같이 받음
    void Die(FVector KillImpulse, FVector HitLocation);

    void SetLastHitInfo(FVector Impulse, FVector HitLocation);


    // [신규] 리플리케이션 될 사망 정보
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Status")
    FDeathDamageInfo LastDeathInfo;

    // [수정] 인자 없이 내부 변수(LastDeathInfo)를 사용하여 처리
    void PerformDeathVisuals();

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

    UFUNCTION(BlueprintCallable, Category = "Status")
    bool IsDead() const;

};