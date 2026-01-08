// WeaponTypes.h
#pragma once

#include "CoreMinimal.h"
#include "WeaponTypes.generated.h"

// 무기 종류
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    OneHand,
    TwoHand,
    None
};

// 무기 데미지 타입 (날카로움 vs 둔탁함)
UENUM(BlueprintType)
enum class EDamageCategory : uint8
{
    Slash_Pierce, // 도검류 (베기/찌르기)
    Blunt         // 둔기류 (타격)
};

// 무기 스탯 구조체 (데이터 테이블용)
USTRUCT(BlueprintType)
struct FWeaponData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh* WeaponMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDamage; // 기본 데미지

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MassKg; // 무기 질량 (물리 연산용)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ArmorPenetration; // 방어구 관통력 (0.0 ~ 1.0)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FleshDamageMultiplier; // 맨몸 추가 데미지 배율

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDamageCategory DamageCategory; // 데미지 타입

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EWeaponType WeaponType; // 무기 타입
};