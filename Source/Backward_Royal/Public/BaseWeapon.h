// BaseWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableInterface.h" // 인터페이스 포함
#include "WeaponTypes.h"
#include "BaseWeapon.generated.h"

UCLASS()
class BACKWARD_ROYAL_API ABaseWeapon : public AActor, public IInteractableInterface
{
    GENERATED_BODY()

public:
    ABaseWeapon();

    // --- 인터페이스 구현 ---
    virtual void Interact(class ABaseCharacter* Character) override;
    virtual FText GetInteractionPrompt() override;

    // --- 무기 고유 기능 ---
    // 캐릭터 손에 맞출 무기 쪽 소켓 이름 (기본값 "Grip")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FName GripSocketName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UStaticMeshComponent* WeaponMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FWeaponData WeaponStats;

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StartAttack();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StopAttack();

    // 히트 스톱을 실행할 함수
    void ApplyHitStop();

    // 장착/해제 시 호출될 이벤트
    virtual void OnEquipped();
    virtual void OnDropped();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnWeaponHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    UPROPERTY()
    TArray<AActor*> HitActors;

private:
    bool bIsEquipped;
};