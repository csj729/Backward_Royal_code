// BaseCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArmorTypes.h" // FArmorData 포함
#include "BaseCharacter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBaseChar, Log, All);

UCLASS()
class BACKWARD_ROYAL_API ABaseCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABaseCharacter();

protected:
    virtual void BeginPlay() override;

    // [Rule] 클래스 내부 단축 로그 매크로
#define CHAR_LOG(Verbosity, Format, ...) UE_LOG(LogBaseChar, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

public:
    // --- Modular Armor Components (파츠별 메쉬) ---
    // GetMesh()는 기본 바디(혹은 알몸)이자 애니메이션의 주체(Leader)로 사용합니다.

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

    // --- Configuration ---
    // true: 방어구 무게가 이동속도에 영향을 줌
    // false: 방어구는 외형(스킨)일 뿐 스탯 영향 없음
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameMode")
    bool bEnableArmorStats;

    // 캐릭터의 기본 이동 속도 (무게 0일 때)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float DefaultWalkSpeed;

    // 현재 장비 총 무게
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
    float CurrentTotalWeight;

    // 부위별 현재 무게 저장 (장비 해제/교체 시 계산용)
    // Key: Slot, Value: Weight
    TMap<EArmorSlot, float> EquippedArmorWeights;

    // --- Functions ---

    // 방어구 장착 함수
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void EquipArmor(EArmorSlot Slot, const FArmorData& NewArmor);

    // 무게에 따른 이동 속도 재계산
    UFUNCTION(BlueprintCallable, Category = "Stats")
    void UpdateMovementSpeedBasedOnWeight();

    // (선택) 방어구 색상 변경 (Material Parameter 조작)
    UFUNCTION(BlueprintCallable, Category = "Customization")
    void SetArmorColor(EArmorSlot Slot, FLinearColor Color);
};