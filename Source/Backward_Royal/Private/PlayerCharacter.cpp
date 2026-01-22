#include "PlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"	
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "BRPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogPlayerChar);

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	// [기본 설정 유지]
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;

	// 1. 카메라 설정
	RearCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("RearCameraBoom"));
	RearCameraBoom->SetupAttachment(RootComponent);
	RearCameraBoom->TargetArmLength = 0.0f;
	RearCameraBoom->bUsePawnControlRotation = false;
	RearCameraBoom->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	RearCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RearCamera"));
	RearCamera->SetupAttachment(RearCameraBoom, USpringArmComponent::SocketName);
	RearCamera->bUsePawnControlRotation = false;

	// 2. 상체 부착 지점
	HeadMountPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HeadMountPoint"));
	HeadMountPoint->SetupAttachment(GetCapsuleComponent());
	HeadMountPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));

	// 3. [신규] 스태미나 컴포넌트 생성
	StaminaComp = CreateDefaultSubobject<UStaminaComponent>(TEXT("StaminaComp"));

	// [네트워크]
	bReplicates = true;
	NetUpdateFrequency = 144.0f;
	MinNetUpdateFrequency = 100.0f;

	GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
}

void APlayerCharacter::BeginPlay()
{
	if (StaminaComp)
	{
		StaminaComp->OnStaminaChanged.AddDynamic(this, &APlayerCharacter::HandleStaminaChanged);
		StaminaComp->OnSprintStateChanged.AddDynamic(this, &APlayerCharacter::HandleSprintStateChanged);
	}

	Super::BeginPlay();

	if (HasAuthority())
	{
		UpperBodyAimRotation = GetActorRotation();
	}

	// [기존 코드] 입력 시스템 등록
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

// 회전값 동기화 수행
void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 로컬 플레이어(내가 조종하는 캐릭터)인 경우에만 실행
	if (IsLocallyControlled())
	{
		FRotator NewRot = GetControlRotation();

		// 내 화면에서는 즉시 적용 (렉 없음)
		UpperBodyAimRotation = NewRot;

		// 서버로 전송 (네트워크 대역폭 절약을 위해 값이 변했을 때만 보내거나, 매 프레임 보냄)
		// 여기서는 부드러운 동기화를 위해 매 프레임 Unreliable RPC로 보냅니다.
		ServerSetAimRotation(NewRot);
	}

	if (!IsLocallyControlled()) // 다른 사람 캐릭터만 표시
	{
		FVector Start = GetActorLocation() + FVector(0, 0, 80);
		FVector End = Start + (UpperBodyAimRotation.Vector() * 100.0f);
		DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, -1.0f, 0, 2.0f);
	}
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 부모 클래스 호출 (안전을 위해)
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Move, Look, Jump 등 기존 바인딩 유지
		if (MoveAction)
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);

		if (LookAction)
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);

		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		// [수정] 달리기
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &APlayerCharacter::SprintStart);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &APlayerCharacter::SprintEnd);
		}
	}
}

// [수정] 컴포넌트에 달리기 요청
void APlayerCharacter::SprintStart(const FInputActionValue& Value)
{
	if (StaminaComp)
	{
		StaminaComp->ServerSetSprinting(true);

	}
}

// [수정] 컴포넌트에 멈춤 요청
void APlayerCharacter::SprintEnd(const FInputActionValue& Value)
{
	if (StaminaComp)
	{
		StaminaComp->ServerSetSprinting(false);
	}
}

// [신규] 컴포넌트가 "달리기 상태 변경"을 알릴 때 호출됨
void APlayerCharacter::HandleSprintStateChanged(bool bCanSprint)
{
	// 스태미나가 바닥나서 bCanSprint가 false로 오거나, 
	// 다시 회복되어 true로 올 수 있음.
	// 하지만 여기서는 "현재 달리고 있는가"에 대한 결과에 따라 속도를 맞춥니다.

	// StaminaComponent 로직에 따라 bIsSprinting 상태를 확인하여 속도 적용
	// (단, 여기서는 파라미터를 단순화해서 bCanSprint가 false면 걷게 함)

	if (StaminaComp && StaminaComp->bIsSprinting)
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}
}

// [신규] 컴포넌트 스태미나 변화 -> 캐릭터 델리게이트로 중계 -> 위젯이 수신
void APlayerCharacter::HandleStaminaChanged(float CurrentVal, float MaxVal)
{
	if (OnStaminaChanged.IsBound())
	{
		OnStaminaChanged.Broadcast(CurrentVal, MaxVal);
	}
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void APlayerCharacter::Jump()
{
	// 스태미나가 충분할 때만 점프 시도 (불필요한 입력 방지)
	if (StaminaComp && StaminaComp->CanJump())
	{
		Super::Jump();
		// [중요] 여기서 ServerConsumeJumpStamina RPC를 호출하지 않습니다!
		// 중복 소모의 원인이 되므로 삭제.
	}
}

// 2. 엔진 내부에서 점프 가능 여부를 판단할 때 (서버/클라이언트 모두 호출됨)
bool APlayerCharacter::CanJumpInternal_Implementation() const
{
	// 기존 조건(바닥에 있는지 등) 체크
	bool bCanJump = Super::CanJumpInternal_Implementation();

	// 스태미나 조건 추가
	if (StaminaComp)
	{
		bCanJump = bCanJump && StaminaComp->CanJump();
	}

	return bCanJump;
}

// 3. 실제로 점프가 이루어졌을 때 (MovementComponent가 호출)
void APlayerCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	// 서버인 경우에만 스태미나 소모
	// (클라이언트의 점프 움직임이 서버에 도달하여 처리될 때 호출됨)
	if (HasAuthority() && StaminaComp)
	{
		StaminaComp->ConsumeJumpStamina();
	}
}

void APlayerCharacter::SetUpperBodyRotation(FRotator NewRotation)
{
	UpperBodyAimRotation = NewRotation;
}

FRotator APlayerCharacter::GetBaseAimRotation() const
{
	// 로컬 컨트롤러가 있는 경우 그냥 컨트롤러 회전값 사용
	if (IsLocallyControlled() && Controller)
	{
		return Controller->GetControlRotation();
	}

	// 다른 클라이언트나 서버의 경우 -> 동기화된 변수 사용
	// 이것이 있어야 "서버"도 플레이어가 어디를 보는지 정확히 알 수 있음 (공격 판정 정확도 상승)
	return UpperBodyAimRotation;
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->Activate();
	}
}

void APlayerCharacter::ServerSetAimRotation_Implementation(FRotator NewRotation)
{
	// 서버에서 변수를 업데이트하면 복제(Replication)를 통해 다른 클라이언트들에게 전파됨
	UpperBodyAimRotation = NewRotation;
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(APlayerCharacter, UpperBodyAimRotation, COND_SkipOwner);
}

void APlayerCharacter::Restart()
{
	Super::Restart();
	if (IsLocallyControlled())
	{
		if (InputComponent)
		{
			SetupPlayerInputComponent(InputComponent);
		}
	}
}

void APlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (IsLocallyControlled())
	{
		if (InputComponent)
		{
			SetupPlayerInputComponent(InputComponent);
		}
		if (ABRPlayerController* PC = Cast<ABRPlayerController>(GetController()))
		{
			PC->SetupRoleInput(true);
		}
	}
}