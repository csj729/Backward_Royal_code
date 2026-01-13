#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BRAttackComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAttackComp, Log, All);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class BACKWARD_ROYAL_API UBRAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBRAttackComponent();

protected:
	virtual void BeginPlay() override;

	// 컴포넌트 내부에서 소유자의 충돌을 처리할 함수
	UFUNCTION()
	void InternalHandleOwnerHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

public:
	// 공격 판정 활성화/비활성화 (애니메이션 노티파이 등에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetAttackDetection(bool bEnabled);

	UFUNCTION(Server, Reliable)
	void ServerSetAttackDetection(bool bEnabled);

	/**
	 * 단일 함수로 모든 데미지 처리
	 * @param DamageMod 데미지 계수 (주먹/무기 스탯 등)
	 */
	void ProcessHitDamage(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit, float DamageMod);

private:
	bool bIsDetectionActive = false;

	UPROPERTY()
	TArray<AActor*> HitActors;

#define ATK_LOG(Verbosity, Format, ...) UE_LOG(LogAttackComp, Verbosity, TEXT("%s: ") Format, *GetOwner()->GetName(), ##__VA_ARGS__)
};