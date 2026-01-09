// ArmorTypes.h
#pragma once

#include "CoreMinimal.h"
#include "ArmorTypes.generated.h"

UENUM(BlueprintType)
enum class EArmorType : uint8
{
    Plate,
    Leather,
    Cloth,
    None
};

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
    EArmorType Type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EArmorSlot EquipSlot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DefensePower; // 방어력

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WeightKg; // 무게 (속도에 영향)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Durability; // 내구도
};