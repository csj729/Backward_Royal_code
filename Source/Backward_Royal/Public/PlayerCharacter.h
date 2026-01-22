#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "InputActionValue.h"
#include "StaminaComponent.h" // 컴포넌트 헤더 포함
#include "PlayerCharacter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPlayerChar, Log, All);

// UI 호환성을 위해 델리게이트 정의 유지
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, CurrentStamina, float, MaxStamina);

UCLASS()
class BACKWARD_ROYAL_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Restart() override;
	virtual void OnRep_PlayerState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Input Functions ---
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	virtual void Jump() override;

	// 엔진이 "점프 가능 여부"를 물어볼 때 스태미나도 체크하도록 오버라이드
	virtual bool CanJumpInternal_Implementation() const override;

	// 실제로 점프가 발생했을 때 호출되는 함수 오버라이드
	virtual void OnJumped_Implementation() override;

	// [수정] 입력 함수가 컴포넌트를 호출하도록 변경
	void SprintStart(const FInputActionValue& Value);
	void SprintEnd(const FInputActionValue& Value);

	// [신규] 컴포넌트로부터 "달리기 불가능/가능" 상태를 전달받는 콜백
	UFUNCTION()
	void HandleSprintStateChanged(bool bCanSprint);

	// [신규] 컴포넌트의 스태미나 변화를 UI로 전달(Relay)하는 콜백
	UFUNCTION()
	void HandleStaminaChanged(float CurrentVal, float MaxVal);

public:
	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaminaComponent* StaminaComp; // 스태미나 관리 컴포넌트

	// --- Camera & Coop ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* RearCameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* RearCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coop")
	class USceneComponent* HeadMountPoint;

	// --- External Setter ---
	void SetUpperBodyRotation(FRotator NewRotation);
	void SetUpperBodyPawn(class AUpperBodyPawn* InPawn) { CurrentUpperBodyPawn = InPawn; }
	virtual FRotator GetBaseAimRotation() const override;

public:
	// --- Input Assets ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* SprintAction;

	// --- Movement Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SprintSpeed = 1000.0f;

	// --- Events ---
	// [유지] 위젯이 이 델리게이트에 바인딩되어 있으므로 유지합니다.
	// 실제 값은 StaminaComp에서 받아와서 뿌려줍니다.
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnStaminaChanged OnStaminaChanged;

	// --- Replicated Variables ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coop|Animation", Replicated)
	FRotator UpperBodyAimRotation;

protected:
	UPROPERTY()
	class AUpperBodyPawn* CurrentUpperBodyPawn;
};