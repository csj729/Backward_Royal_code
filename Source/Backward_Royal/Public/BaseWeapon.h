// BaseWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableInterface.h"
#include "WeaponTypes.h"
#include "BaseWeapon.generated.h"

UCLASS()
class BACKWARD_ROYAL_API ABaseWeapon : public AActor, public IInteractableInterface
{
    GENERATED_BODY()

public:
    ABaseWeapon();

    /** 무기 밸런스 계수 설정 */

    // 데미지 계산 시 곱해지는 계수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon | Stats")
    float DamageCoefficient;

    // 충격량(물리적 밀어내기) 계산 시 곱해지는 계수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon | Stats")
    float ImpulseCoefficient;

    // 애니메이션 재생 속도 조절용 계수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon | Stats")
    float AttackSpeedCoefficient;

    // --- 인터페이스 구현 ---
    virtual void Interact(class ABaseCharacter* Character) override;
    virtual FText GetInteractionPrompt() override;

    // --- 무기 데이터 및 메시 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UStaticMeshComponent* WeaponMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FName WeaponRowName;

    // 로드된 데이터를 저장할 변수 (내부 로직용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    FWeaponData CurrentWeaponData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FName GripSocketName;

    // [추가] 데이터 테이블 등에서 가져온 정보를 실제 컴포넌트에 적용하는 함수
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void InitializeWeaponStats(const FWeaponData& NewStats);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StartAttack();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StopAttack();

    void ApplyHitStop();

    virtual void OnEquipped();
    virtual void OnDropped();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override; // 에디터에서 수치 변경 시 즉시 반영

    // 데이터 테이블에서 수치를 읽어오는 헬퍼 함수
    void LoadWeaponData();

    UFUNCTION()
    void OnWeaponHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    UPROPERTY()
    TArray<AActor*> HitActors;

private:
    bool bIsEquipped;
};