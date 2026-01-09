// SwitchOrb.h
#pragma once

#include "CoreMinimal.h"
#include "DropItem.h"
#include "SwitchOrb.generated.h"

// 로그 카테고리 선언
DECLARE_LOG_CATEGORY_EXTERN(LogSwitchOrb, Log, All);

class UNiagaraComponent;

UCLASS()
class BACKWARD_ROYAL_API ASwitchOrb : public ADropItem
{
    GENERATED_BODY()

public:
    ASwitchOrb();

protected:
    virtual void BeginPlay() override;

    // 커스텀 디버그 로그 매크로
#define ORB_LOG(Verbosity, Format, ...) UE_LOG(LogSwitchOrb, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

// 오버랩 감지를 위한 충돌체
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* CollisionSphere;

    // 나이아가라 이펙트 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UNiagaraComponent* OrbNiagaraComp;

    // 오버랩 이벤트 핸들러
    UFUNCTION()
    void OnOrbOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);
};