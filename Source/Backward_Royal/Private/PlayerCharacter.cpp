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

	// [추가] 서버 권한이 있을 때, 시작 시점의 상체 회전을 현재 몸통 회전으로 초기화
	// 이 코드가 없으면 스폰 직후에 고개가 (0,0,0) 북쪽을 보며 꺾여있을 수 있습니다.
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

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// [서버 권한] 이동 모드 초기화 및 물리 상태 활성화
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->Activate();
	}
}

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 조건 없이 모든 클라이언트(주인 포함)에게 복제되도록 수정
	DOREPLIFETIME(APlayerCharacter, UpperBodyAimRotation);
}

void APlayerCharacter::Restart()
{
	Super::Restart();

	// 클라이언트에서 컨트롤러 할당이 완료된 직후 호출됨
	if (IsLocallyControlled())
	{
		// 입력 컴포넌트가 있다면 바인딩 로직을 다시 실행하여 함수 연결
		if (InputComponent)
		{
			SetupPlayerInputComponent(InputComponent);
		}

		UE_LOG(LogTemp, Warning, TEXT("재설정"));
	}
}

void APlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// PlayerState가 복제되어 들어왔을 때 (컨트롤러 소유권 확인 시점)
	if (IsLocallyControlled())
	{
		if (InputComponent)
		{
			SetupPlayerInputComponent(InputComponent);
			UE_LOG(LogTemp, Warning, TEXT("OnRep_PlayerState를 통해 입력 바인딩 재설정"));
		}

		if (ABRPlayerController* PC = Cast<ABRPlayerController>(GetController()))
		{
			PC->SetupRoleInput(true); // 하체용 IMC 강제 로드
		}
	}
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (!PlayerInputComponent) return;

	PlayerInputComponent->ClearActionBindings();

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
		// 1. 컨트롤러의 회전값 중 Yaw(수평 회전)만 가져옵니다.
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// 2. 전방(Forward) 벡터 구하기 (W/S 이동용)
		// EAxis::X 는 앞쪽 방향을 의미합니다.
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// 3. 우측(Right) 벡터 구하기 (A/D 이동용) [새로 추가된 부분]
		// EAxis::Y 는 오른쪽 방향을 의미합니다.
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 4. 입력값 적용
		// W/S 입력(MovementVector.Y)은 앞뒤 벡터에 적용
		AddMovementInput(ForwardDirection, MovementVector.Y);

		// A/D 입력(MovementVector.X)은 좌우 벡터에 적용
		// 기존의 AddControllerYawInput(회전) 대신 AddMovementInput(이동)을 사용합니다.
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

void APlayerCharacter::SetUpperBodyRotation(FRotator NewRotation)
{
	UpperBodyAimRotation = NewRotation;
}

FRotator APlayerCharacter::GetBaseAimRotation() const
{
	// 원래는 컨트롤러(하체 플레이어)의 회전을 가져오지만,
	// 우리는 상체 플레이어가 정해준 회전값(UpperBodyAimRotation)을 강제로 리턴합니다.
	return UpperBodyAimRotation;
}