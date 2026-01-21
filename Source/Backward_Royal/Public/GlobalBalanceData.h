#pragma once

#include "CoreMinimal.h"
#include "GlobalBalanceData.generated.h"

// 전역 밸런스 데이터 구조체
USTRUCT(BlueprintType)
struct FGlobalBalanceData : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Weapon_DamageMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Weapon_ImpulseMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Global_Weapon_AttackSpeedMultiplier = 1.0f;
};