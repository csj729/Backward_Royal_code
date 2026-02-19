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

	static float BasePunchDamage;

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
	void ProcessHitDamage(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit);

	/** 현재 상태(무기 유무 등)를 기반으로 최종 공격 속도를 계산합니다. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float GetCalculatedAttackSpeed() const;

	/** 공격 속도 기준 무게 (C# 툴의 영향을 받지 않는 내부 보정치) */
	UPROPERTY(EditAnywhere, Category = "Combat|Settings")
	float StandardMass = 10.0f;

	// 히트 스탑(역경직) 적용 함수
	void ApplyHitStop(float Duration);

	// 서버에서 모든 클라이언트로 히트 스탑 명령 전송
	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplyHitStop(float Duration);

private:
	bool bIsDetectionActive = false;

	UPROPERTY()
	TArray<AActor*> HitActors;

	// 히트 스탑 타이머 핸들
	FTimerHandle HitStopTimerHandle;

	// 히트 스탑 해제 및 애니메이션 종료 함수
	void ResetHitStop();

#define ATK_LOG(Verbosity, Format, ...) UE_LOG(LogAttackComp, Verbosity, TEXT("%s: ") Format, *GetOwner()->GetName(), ##__VA_ARGS__)
};