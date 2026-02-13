#pragma once

#include "CoreMinimal.h"
#include "CustomizationInfo.generated.h"

USTRUCT(BlueprintType)
struct FBRCustomizationData
{
    GENERATED_BODY()

    // 상체 파츠 (Upper Player가 결정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HeadID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ChestID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HandID = 0;

    // 하체 파츠 (Lower Player가 결정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LegID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 FootID = 0;

    // 필요 시 색상 정보도 추가 가능
    // UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // FLinearColor PrimaryColor;
};