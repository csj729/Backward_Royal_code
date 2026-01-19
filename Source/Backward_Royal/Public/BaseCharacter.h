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

    // --- Stats ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float DefaultWalkSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MaxHP = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_CurrentHP)
    float CurrentHP;

    UFUNCTION()
    void OnRep_CurrentHP();

    // =================================================================
    // [전투 시스템]
    // =================================================================

    // 무기 공격 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    UAnimMontage* AttackMontage;

    // [신규] 맨손 콤보 몽타주 (Combo1, Combo2, Combo3 섹션 필요)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    UAnimMontage* UnarmedComboMontage;

    // [신규] 공격 요청 처리 (서버에서 호출됨)
    void RequestAttack();

    // 기존 무기 공격 멀티캐스트
    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayAttack(APawn* RequestingPawn);

    // [신규] 맨손 콤보 멀티캐스트
    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayUnarmedCombo(int32 SectionIndex);

    // [신규] 애니메이션 노티파이용 함수 (BlueprintCallable 필수)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SetComboInputWindow(bool bEnable); // 입력 허용 구간 열기/닫기

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void CheckNextCombo(); // 다음 콤보로 넘어갈지 결정

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
    // [신규] 콤보 관련 상태 변수
    int32 CurrentComboIndex = 0;
    int32 MaxComboCount = 2;

    // 캐릭터 자체의 공격 상태 플래그
    bool bIsCharacterAttacking = false;

    // 입력 버퍼링용 플래그
    bool bIsComboInputOn = false;      // 입력 허용 구간인가?
    bool bIsNextComboReserved = false; // 다음 공격이 예약되었는가?
};