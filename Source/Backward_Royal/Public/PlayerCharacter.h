#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "InputActionValue.h"
#include "PlayerCharacter.generated.h"

// 로그 카테고리 선언
DECLARE_LOG_CATEGORY_EXTERN(LogPlayerChar, Log, All);

UCLASS()
class BACKWARD_ROYAL_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	// 상체(Player A)가 이 캐릭터의 상반신 회전값을 업데이트할 때 호출
	void SetUpperBodyRotation(FRotator NewRotation);

	// 애니메이션 블루프린트에서 사용할 변수
	UPROPERTY(BlueprintReadOnly, Category = "Coop|Animation")
	FRotator UpperBodyAimRotation;

	// 상체 Pawn을 저장하고 관리하기 위한 함수 및 변수
	void SetUpperBodyPawn(class AUpperBodyPawn* InPawn) { CurrentUpperBodyPawn = InPawn; }

	void Restart() override;
	virtual void OnRep_PlayerState() override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	// 이동 (Player B 전용)
	void Move(const FInputActionValue& Value);
	// 시선 (Player B 전용 - 후방 카메라 회전)
	void Look(const FInputActionValue& Value);

	UPROPERTY()
	class AUpperBodyPawn* CurrentUpperBodyPawn;

public:
	// --- Camera (Rear View) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* RearCameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* RearCamera;

	// --- Mount Point ---
	// [중요] 기존 UpperBodyMountPoint 삭제 -> HeadMountPoint로 대체
	// Player A(상반신) Pawn이 부착될 위치
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
};