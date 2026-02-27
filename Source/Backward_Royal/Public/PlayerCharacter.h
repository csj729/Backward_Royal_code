#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "InputActionValue.h"
#include "StaminaComponent.h"
#include "CustomizationInfo.h"
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
    virtual void OnRep_PlayerState() override;

    // 플레이어 이동 관련 전역 변수 (GameInstance에서 업데이트됨)
    static float Global_RotationRateYaw;
    static float Global_BrakingFriction;
    static float Global_BrakingDecelerationWalking;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void Restart() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    
    // 발자국 소리 처리 함수
    void ProcessFootstep(float DeltaTime);

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

    // [신규] 커스터마이징 적용 시도 함수
    UFUNCTION()
    void TryApplyCustomization();

    // [신규] 파트너가 유효할 때 파트너의 이벤트를 바인딩하는 함수
    UFUNCTION()
    void BindToPartnerPlayerState(bool bIsLowerBody);

    // 특정 메쉬 교체 적용 (ID를 받아서 매칭되는 SkeletalMesh 적용)
    void ApplyMeshFromID(EArmorSlot Slot, int32 MeshID);

    // 상체/하체 PlayerState 찾기 헬퍼
    class ABRPlayerState* GetUpperBodyPlayerState() const;
    class ABRPlayerState* GetLowerBodyPlayerState() const;

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

    // UI에서 호출하여 로비 캐릭터의 미리보기 메쉬를 업데이트하는 함수
    UFUNCTION(BlueprintCallable, Category = "Customization")
    void UpdatePreviewMesh(const FBRCustomizationData& NewData);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Defaults")
    USkeletalMesh* DefaultHeadMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Defaults")
    USkeletalMesh* DefaultChestMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Defaults")
    USkeletalMesh* DefaultHandMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Defaults")
    USkeletalMesh* DefaultLegMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|Defaults")
    USkeletalMesh* DefaultFootMesh;

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

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnStaminaChanged OnStaminaChanged;

    // --- Replicated Variables ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coop|Animation", Replicated)
    FRotator UpperBodyAimRotation;
    
    // 발자국 소리 파일
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    class USoundBase* FootstepSound;

    // 발자국 소리 크기 (1.0 = 기본)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    float FootstepVolume = 1.0f;
    
    // 발자국 소리 간격 (예: 150cm마다 소리 재생)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
    float FootstepDistanceThreshold = 150.0f;

protected:
    UPROPERTY()
    class AUpperBodyPawn* CurrentUpperBodyPawn;
    
private:
    // 이동 거리 누적 변수 (중복 제거됨)
    float AccumulatedDistance = 0.0f;

    FTimerHandle TimerHandle_RetryBindPartner;

    // 초기 외형 설정 완료 후 중복 적용 방지 플래그
    bool bAppearanceLocked = false;

    // 파트너 PlayerState에 바인딩되었는지 체크
    bool bBoundToPartner = false;

    /** 전원 스폰 완료 후 Move()에서 컨트롤러 이동 입력 해제를 1회만 수행했는지 */
    bool bMoveInputUnblocked = false;
};