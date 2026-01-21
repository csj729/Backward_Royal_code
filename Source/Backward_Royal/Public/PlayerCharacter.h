#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "InputActionValue.h"
#include "PlayerCharacter.generated.h"

// 로그 카테고리 선언
DECLARE_LOG_CATEGORY_EXTERN(LogPlayerChar, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, CurrentStamina, float, MaxStamina);

UCLASS()
class BACKWARD_ROYAL_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	// -------------------------------------------------------------------
	// [공용/외부 접근 가능 함수]
	// -------------------------------------------------------------------

	// 상체(Player A)가 이 캐릭터의 상반신 회전값을 업데이트할 때 호출
	void SetUpperBodyRotation(FRotator NewRotation);

	// 상체 Pawn을 저장하고 관리하기 위한 함수
	void SetUpperBodyPawn(class AUpperBodyPawn* InPawn) { CurrentUpperBodyPawn = InPawn; }

	// [중요] 부모 클래스(APawn, ACharacter)에서 Public인 함수들은 
	// 여기서도 Public에 두는 것이 안전합니다. (외부 클래스 접근 이슈 방지)
	virtual void Restart() override;
	virtual void OnRep_PlayerState() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ★ 핵심: 시선 방향 계산 재정의 (상체 플레이어의 시선을 따르도록 함)
	virtual FRotator GetBaseAimRotation() const override;

protected:
	// -------------------------------------------------------------------
	// [내부 로직 / 상속용 함수]
	// -------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 이동 (Player B 전용)
	void Move(const FInputActionValue& Value);
	// 시선 (Player B 전용 - 후방 카메라 회전)
	void Look(const FInputActionValue& Value);

	UPROPERTY()
	class AUpperBodyPawn* CurrentUpperBodyPawn;

public:
	// -------------------------------------------------------------------
	// [변수 / 컴포넌트]
	// -------------------------------------------------------------------

	// 애니메이션 블루프린트에서 사용할 변수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coop|Animation", Replicated)
	FRotator UpperBodyAimRotation;

	// --- Camera (Rear View) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* RearCameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* RearCamera;

	// --- Mount Point ---
	// Player A(상반신) Pawn이 부착될 위치
	// (팀원이 무기 장착 등에 이 컴포넌트를 참조할 수 있으므로 유지해야 함)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coop")
	class USceneComponent* HeadMountPoint;

	// --- Inputs (Player B) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxStamina = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", ReplicatedUsing = OnRep_CurrentStamina)
	float CurrentStamina;

	UFUNCTION(BlueprintCallable)
	void OnRep_CurrentStamina();

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnStaminaChanged OnStaminaChanged;

	// 스태미나 갱신 헬퍼 (스태미나 소비 함수가 있다면 거기서도 호출하세요)
	void UpdateStaminaUI();
};