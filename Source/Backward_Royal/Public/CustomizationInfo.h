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

    // 데이터가 유효한지 체크 (전송 시 true로 설정 필수)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsDataValid = false;
};