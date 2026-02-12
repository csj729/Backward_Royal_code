// SwitchOrb.h
#pragma once

#include "CoreMinimal.h"
#include "DropItem.h"
#include "SwitchOrb.generated.h"

class UNiagaraComponent;

UCLASS()
class BACKWARD_ROYAL_API ASwitchOrb : public ADropItem
{
    GENERATED_BODY()

public:
    ASwitchOrb();

protected:
    virtual void BeginPlay() override;

    // ????? ????? ??? ?????

// ?????? ?????? ???? ??�
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* CollisionSphere;

    // ???????? ????? ???????
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UNiagaraComponent* OrbNiagaraComp;

    // ?????? ???? ???
    UFUNCTION()
    void OnOrbOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);
};