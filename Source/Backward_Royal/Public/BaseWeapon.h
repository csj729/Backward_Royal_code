// BaseWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponTypes.h"
#include "InteractableInterface.h" // 인터페이스 추가
#include "BaseWeapon.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBaseWeapon, Log, All);

UCLASS()
class BACKWARD_ROYAL_API ABaseWeapon : public AActor, public IInteractableInterface
{
    GENERATED_BODY()

public:
    ABaseWeapon();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    void InitializeDropPhysics();

#define WEAPON_LOG(Verbosity, Format, ...) UE_LOG(LogBaseWeapon, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

public:
    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* WeaponMesh;

    // --- Configuration ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    FWeaponData WeaponStats;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
    TArray<FName> TraceSocketNames;

    UPROPERTY(EditDefaultsOnly, Category = "Collision")
    TEnumAsByte<ECollisionChannel> TraceChannel;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bShowDebugTrace;

    // --- State ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsAttacking;

    // 현재 누군가에게 장착된 상태인지 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsEquipped;

    UPROPERTY()
    TArray<AActor*> HitActors;

private:
    TMap<FName, FVector> PreviousSocketLocations;
    void PerformWeaponTrace();
    float CalculatePhysicsDamage(const FHitResult& HitResult, FName HitSocketName, float ImpactSpeed);

public:
    // --- Interface Implementation ---
    virtual void Interact(class ABaseCharacter* Character) override;
    virtual FText GetInteractionPrompt() override;

    // --- Functions ---
    // 무기를 손에 쥐었을 때 (물리 OFF)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void OnEquipped();

    // 무기를 바닥에 버렸을 때 (물리 ON)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void OnDropped();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StartAttack();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EndAttack();
};