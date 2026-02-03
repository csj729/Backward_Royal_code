// ArmorTypes.h
#pragma once

#include "CoreMinimal.h"
#include "ArmorTypes.generated.h"

UENUM(BlueprintType)
enum class EArmorSlot : uint8
{
    Head = 0,   // 100번대
    Chest,      // 200번대
    Hands,      // 300번대
    Legs,       // 400번대
    Feet,       // 500번대
    None        // 예외
};

USTRUCT(BlueprintType)
struct FArmorData : public FTableRowBase
{
    GENERATED_BODY()

    // ID 규칙: (Slot + 1) * 100 + Index
    // 예: Head(0) -> 101, Chest(1) -> 201
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USkeletalMesh* ArmorMesh; // 적용할 메쉬

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EArmorSlot EquipSlot;

    FArmorData()
        : ID(0), DisplayName(NAME_None), ArmorMesh(nullptr), EquipSlot(EArmorSlot::None)
    {
    }

    // --- [Helper] ID 규칙 관련 함수 ---

    /** ID를 기반으로 해당 아이템의 슬롯 타입을 유추합니다. */
    static EArmorSlot GetSlotFromID(int32 InID)
    {
        // 100 미만이나 범위 밖은 None 처리
        if (InID < 100) return EArmorSlot::None;

        // 백의 자리수 추출 (153 -> 1, 205 -> 2)
        int32 SlotPrefix = InID / 100;

        // 규칙: Prefix = Slot + 1 이므로, Slot = Prefix - 1
        int32 SlotIndex = SlotPrefix - 1;

        if (SlotIndex >= 0 && SlotIndex <= (int32)EArmorSlot::Feet)
        {
            return (EArmorSlot)SlotIndex;
        }

        return EArmorSlot::None;
    }

    /** 해당 ID가 이 슬롯에 맞는 유효한 ID인지 검사합니다. */
    static bool IsValidIDForSlot(int32 InID, EArmorSlot InSlot)
    {
        return GetSlotFromID(InID) == InSlot;
    }
};

