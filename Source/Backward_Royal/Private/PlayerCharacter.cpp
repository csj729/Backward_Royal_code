#include "PlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"	
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogPlayerChar);

APlayerCharacter::APlayerCharacter()
{
	// [조작 설정] 탱크/말 방식 (몸통 회전 O, 이동 방향 회전 X)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;

	// 1. 후방 카메라 설정 (Player B)
	RearCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("RearCameraBoom"));
	RearCameraBoom->SetupAttachment(RootComponent);
	RearCameraBoom->TargetArmLength = 0.0f; // 1인칭 느낌

	// 말 운전자는 시점 고정 (컨트롤러 회전 안 따라감)
	RearCameraBoom->bUsePawnControlRotation = false;
	RearCameraBoom->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	RearCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RearCamera"));
	RearCamera->SetupAttachment(RearCameraBoom, USpringArmComponent::SocketName);
	RearCamera->bUsePawnControlRotation = false;

	// 2. 상체(Player A) 부착 지점 생성
	HeadMountPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HeadMountPoint"));

	// [오류 해결됨] 이제 캡슐 컴포넌트 헤더가 있어서 정상적으로 붙습니다.
	HeadMountPoint->SetupAttachment(GetCapsuleComponent());

	// 캡슐 중앙(배꼽)에서 눈높이(Z +65)만큼 위로 올림
	HeadMountPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));

	// =================================================================
	// [네트워크] 업데이트 빈도 증가
	// =================================================================
	bReplicates = true;
	NetUpdateFrequency = 144.0f;
	MinNetUpdateFrequency = 100.0f;

	GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

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

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);

		if (LookAction)
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);

		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
	}
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// W/S: 전후 이동
		if (MovementVector.Y != 0.0f)
		{
			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

			AddMovementInput(ForwardDirection, MovementVector.Y);
		}

		// A/D: 제자리 회전
		if (MovementVector.X != 0.0f)
		{
			AddControllerYawInput(MovementVector.X);
		}
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

void APlayerCharacter::SetUpperBodyRotation(FRotator NewRotation)
{
	UpperBodyAimRotation = NewRotation;
}