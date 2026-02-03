#pragma once

#include "CoreMinimal.h"
#include "GlobalBalanceData.generated.h"

// 전역 밸런스 데이터 구조체
USTRUCT(BlueprintType)
struct FGlobalBalanceData : public FTableRowBase
{
    GENERATED_BODY()

public:
    // --- 무기 관련 설정 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Weapon_DamageMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Weapon_ImpulseMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Weapon_AttackSpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Durability_Reduction = 10.f;

    // --- 스태미나 관련 설정 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Stamina_SprintDrainRate = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Stamina_JumpCost = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Stamina_RegenRate = 10.0f;
};