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

    UFUNCTION(NetMulticast, Reliable)
    void MulticastDie();

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

    //UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics")
    //UPhysicsControlComponent* PhysicsControlComp;

    // --- Stats ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float DefaultWalkSpeed;

    // 블루프린트 UI에서 바인딩할 변수
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnHPChanged OnHPChanged;

    // 체력이 변할 때 공통적으로 호출할 헬퍼 함수
    void UpdateHPUI();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MaxHP = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_CurrentHP)
    float CurrentHP;

    UFUNCTION(BlueprintCallable)
    void OnRep_CurrentHP();

    // =================================================================
    // [전투 시스템]
    // =================================================================

    // 무기 공격 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    UAnimMontage* AttackMontage;
    // 펀치 몽타주
    UPROPERTY(EditAnywhere, Category = "Combat")
    UAnimMontage* PunchMontage_L;

    UPROPERTY(EditAnywhere, Category = "Combat")
    UAnimMontage* PunchMontage_R;

    // 다음 공격이 왼손인지 확인하는 플래그
    bool bNextAttackIsLeft = false;

    // [신규] 공격 요청 처리 (서버에서 호출됨)
    void RequestAttack();

    // 기존 무기 공격 멀티캐스트
    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayWeaponAttack(APawn* RequestingPawn);

    // 공격 실행 (몽타주 기반)
    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayPunch(UAnimMontage* TargetMontage);

    UFUNCTION(BlueprintCallable)
    void EnhanceFistPhysics(bool bEnable);

    // 데미지 처리
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
    // 캐릭터 자체의 공격 상태 플래그
    bool bIsCharacterAttacking = false;
    
};