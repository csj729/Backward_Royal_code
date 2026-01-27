#pragma once

#include "CoreMinimal.h"
#include "PlayerCharacter.h" // 부모 클래스
#include "Misc/Optional.h"   // 에러 방지용 필수 헤더
#include "SoloTesterCharacter.generated.h"

// 전방 선언
class AUpperBodyPawn;
class UInputMappingContext;
class UInputAction;
class UAnimMontage;

UCLASS()
class BACKWARD_ROYAL_API ASoloTesterCharacter : public APlayerCharacter
{
	GENERATED_BODY()

public:
	ASoloTesterCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;

public:
	// 상체 블루프린트 (카메라 역할)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Setup")
	TSubclassOf<AUpperBodyPawn> UpperBodyClass;

	// 공격 키 (IA_Attack)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* TestAttackAction;

	// 상호작용 키 (IA_Interact)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* TestInteractAction;

	// 실제 스폰된 상체
	UPROPERTY(VisibleInstanceOnly, Category = "Test Setup")
	AUpperBodyPawn* UpperBodyInstance;

protected:
	// 공격 실행 함수
	UFUNCTION(BlueprintCallable)
	void RelayAttack(const FInputActionValue& Value);

	// 상호작용 실행 함수
	void RelayInteract(const FInputActionValue& Value);

	// 물리 충돌 감지 (★ 이 부분이 없어서 에러가 났던 것입니다)
	UFUNCTION()
	void OnAttackHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 블루프린트 강제 호출용
	UFUNCTION(BlueprintCallable)
	void ForceAttack();
};