#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Misc/Optional.h"
#include "UpperBodyPawn.generated.h"

// 전방 선언
class APlayerCharacter;
class UInputMappingContext;
class UInputAction;
class UAnimMontage; // [수정] 몽타주 클래스 인식용

UCLASS()
class BACKWARD_ROYAL_API AUpperBodyPawn : public APawn
{
	GENERATED_BODY()

public:
	AUpperBodyPawn();

protected:
	virtual void BeginPlay() override;

	// 매 프레임마다 몸통 회전을 따라가기 위해 Tick이 필요합니다.
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Look(const FInputActionValue& Value);
	void Attack(const FInputActionValue& Value);
	void Interact(const FInputActionValue& Value);

	virtual void OnRep_PlayerState() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* FrontCameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* FrontCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputMappingContext* UpperBodyMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* InteractAction;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractionDistance = 300.0f;

	// [수정] 이 변수가 없어서 TestSoloCharacter에서 에러가 났었습니다. 추가 필수!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimMontage* AttackMontage;

	// 클라이언트에서 서버로 공격 상태 변경을 요청하는 RPC
	UFUNCTION(Server, Reliable)
	void ServerRequestSetAttackDetection(bool bEnabled);

	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(AActor* TargetActor);

	UFUNCTION()
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UPROPERTY(BlueprintReadOnly)
	class APlayerCharacter* ParentBodyCharacter;
	
	UFUNCTION(Server, Unreliable) // 자주 호출되므로 Unreliable 권장
		void ServerUpdateAimRotation(FRotator NewRotation);

private:
	// 지난 프레임의 몸통 각도를 저장할 변수
	float LastBodyYaw;
};