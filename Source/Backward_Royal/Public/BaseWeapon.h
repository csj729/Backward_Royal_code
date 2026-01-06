// BaseWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponTypes.h"
#include "BaseWeapon.generated.h"

// [Rule] 클래스 전용 로그 카테고리 선언
DECLARE_LOG_CATEGORY_EXTERN(LogBaseWeapon, Log, All);

UCLASS()
class BACKWARD_ROYAL_API ABaseWeapon : public AActor
{
    GENERATED_BODY()

public:
    ABaseWeapon();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // [Rule] 클래스 내부에서 사용할 단축 매크로 정의
#define WEAPON_LOG(Verbosity, Format, ...) UE_LOG(LogBaseWeapon, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

public:
    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* WeaponMesh;

    // --- Configuration ---
    // 무기 스탯 (공격력, 질량, 관통력 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    FWeaponData WeaponStats;

    // 충돌 검사를 수행할 소켓 이름 목록 (Blueprint에서 설정: Trace_01, Trace_02...)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
    TArray<FName> TraceSocketNames;

    // 트레이스 채널 (캐릭터 감지용, 기본 Pawn)
    UPROPERTY(EditDefaultsOnly, Category = "Collision")
    TEnumAsByte<ECollisionChannel> TraceChannel;

    // 디버그 라인 표시 여부
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bShowDebugTrace;

    // --- State ---
    // 현재 공격 동작 수행 중인지 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsAttacking;

    // 한 번의 휘두르기에서 이미 타격된 액터 목록 (중복 데미지 방지)
    UPROPERTY()
    TArray<AActor*> HitActors;

private:
    // 이전 프레임의 소켓 위치들을 저장 (속도 계산 및 트레이스 시작점용)
    TMap<FName, FVector> PreviousSocketLocations;

    // 내부 로직 함수들
    void PerformWeaponTrace();
    float CalculatePhysicsDamage(const FHitResult& HitResult, FName HitSocketName, float ImpactSpeed);

public:
    // --- Interface for Animation Notifies ---

    // 공격 시작 (Anim Notify State Begin 등에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StartAttack();

    // 공격 종료 (Anim Notify State End 등에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EndAttack();
};