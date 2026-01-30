#pragma once

#include "CoreMinimal.h"
#include "PlayerCharacter.h" // 부모 클래스 (이미 모든 전투 기능 보유)
#include "Misc/Optional.h"   // 필수 헤더
#include "SoloTesterCharacter.generated.h"

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

	// 스폰된 상체
	UPROPERTY(VisibleInstanceOnly, Category = "Test Setup")
	AUpperBodyPawn* UpperBodyInstance;

protected:
	// [핵심] 부모의 공격 함수 호출
	void RelayAttack(const FInputActionValue& Value);

	// 상호작용 (무기 줍기용)
	void RelayInteract(const FInputActionValue& Value);

	// 블루프린트 테스트용
	UFUNCTION(BlueprintCallable)
	void ForceAttack();
};