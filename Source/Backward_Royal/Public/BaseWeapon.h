#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseWeapon.generated.h" // BoxComponent 헤더 필요 없음

UCLASS()
class BACKWARD_ROYAL_API ABaseWeapon : public AActor
{
    GENERATED_BODY()

public:
    ABaseWeapon();

protected:
    virtual void BeginPlay() override;

public:
    // 이제 이 메시가 '외형'이자 '충돌체'입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UStaticMeshComponent* WeaponMesh;

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StartAttack();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void StopAttack();

protected:
    // BoxComponent가 아니라 StaticMeshComponent의 Hit을 받습니다.
    UFUNCTION()
    void OnWeaponHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit
    );
};