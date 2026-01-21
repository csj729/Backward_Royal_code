// ArmorTypes.h
#pragma once

#include "CoreMinimal.h"
#include "ArmorTypes.generated.h"

UENUM(BlueprintType)
enum class EArmorSlot : uint8
{
    Head,   // 머리
    Chest,  // 상체 (갑옷)
    Hands,  // 장갑
    Legs,   // 하의
    Feet,   // 신발
    None    // 없음
};

USTRUCT(BlueprintType)
struct FArmorData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USkeletalMesh* ArmorMesh; // 적용할 메쉬

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EArmorSlot EquipSlot;
};