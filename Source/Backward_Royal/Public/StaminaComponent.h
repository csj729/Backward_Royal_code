#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StaminaComponent.generated.h"

// UI 업데이트용 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaValueUpdated, float, CurrentStamina, float, MaxStamina);

// 상태 변경 알림용 델리게이트 (예: 지침, 회복됨)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSprintStateChanged, bool, bCanSprint);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class BACKWARD_ROYAL_API UStaminaComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStaminaComponent();

    static float Global_SprintDrainRate;
    static float Global_JumpCost;
    static float Global_RegenRate;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // [서버] 달리기 요청 처리
    UFUNCTION(BlueprintCallable, Server, Reliable)
    void ServerSetSprinting(bool bNewSprinting);

    // 점프 시 스태미나 소모
    UFUNCTION(BlueprintCallable)
    void ConsumeJumpStamina();

    // 현재 스태미나가 충분한지 확인
    UFUNCTION(BlueprintCallable)
    bool CanJump() const { return CurrentStamina >= JumpCost; }

    // 현재 스태미나 비율 반환 (0.0 ~ 1.0)
    UFUNCTION(BlueprintCallable)
    float GetStaminaRatio() const;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
    float MaxStamina;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentStamina, VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
    float CurrentStamina;

    UPROPERTY(EditAnywhere, Category = "Stamina")
    float StaminaDrainRate;

    UPROPERTY(EditAnywhere, Category = "Stamina")
    float StaminaRegenRate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
    float JumpCost;

    UPROPERTY(ReplicatedUsing = OnRep_IsSprinting, VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
    bool bIsSprinting = false;

    UFUNCTION()
    void OnRep_IsSprinting();

    // 델리게이트
    UPROPERTY(BlueprintAssignable)
    FOnStaminaValueUpdated OnStaminaChanged;

    UPROPERTY(BlueprintAssignable)
    FOnSprintStateChanged OnSprintStateChanged;

    UFUNCTION()
    void OnRep_CurrentStamina();
};