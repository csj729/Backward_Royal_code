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
#include "BRPlayerState.h"
#include "BRGameState.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogPlayerChar);

#define LOG_PLAYER(Verbosity, Format, ...) \
    UE_LOG(LogPlayerChar, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(Format, ##__VA_ARGS__))

APlayerCharacter::APlayerCharacter()
{
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
	SetNetUpdateFrequency(144.0f);
	SetMinNetUpdateFrequency(100.0f);

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

	if (GetPlayerState())
	{
		OnRep_PlayerState();
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

		// 카메라 기준 앞/오른쪽 방향
		const FVector CameraForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector CameraRight = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// [신규 로직 시작] ----------------------------------------------------------

		// 1. 플레이어가 입력한 키(W,A,S,D)가 실제 월드에서 어느 방향인지 계산
		FVector IntentDir = (CameraForward * MovementVector.Y + CameraRight * MovementVector.X).GetSafeNormal();

		// 2. 내적(Dot Product) 계산
		// 캐릭터의 정면(GetActorForwardVector)과 이동하려는 방향(IntentDir)을 비교
		// 결과가 양수(+)면 캐릭터가 보는 방향으로 걷는 것이고, 음수(-)면 뒷걸음질 치는 것
		float Dot = FVector::DotProduct(IntentDir, GetActorForwardVector());

		float SpeedScale = 1.0f;

		// 3. 캐릭터가 바라보는 방향으로 움직일 때 (내적이 0보다 클 때)
		// 현재 캐릭터는 카메라 등 뒤를 보고 있으므로, S키를 눌러 카메라 쪽으로 올 때가 여기에 해당됨
		if (Dot > 0.1f)
		{
			SpeedScale = 0.5f; // 0.5배속 적용
		}
		// [신규 로직 끝] ------------------------------------------------------------

		// 계산된 배율(SpeedScale)을 곱해서 이동 적용
		AddMovementInput(CameraForward, MovementVector.Y * SpeedScale);
		AddMovementInput(CameraRight, MovementVector.X * SpeedScale);
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

void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APlayerCharacter, UpperBodyAimRotation);
}

void APlayerCharacter::Restart()
{
	Super::Restart();
}

void APlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (IsLocallyControlled())
	{
		if (ABRPlayerController* PC = Cast<ABRPlayerController>(GetController()))
		{
			PC->SetupRoleInput(true);
		}
	}

	ABRPlayerState* MyPS = Cast<ABRPlayerState>(GetPlayerState());
	if (MyPS)
	{
		// 1-1. 내 커마 정보가 오면 알려줘
		MyPS->OnCustomizationDataChanged.AddDynamic(this, &APlayerCharacter::TryApplyCustomization);

		// 1-2. 내 역할(상/하체)이나 파트너가 정해지면 알려줘
		// (기존 코드에 OnPlayerRoleChanged 델리게이트가 이미 있다고 가정)
		MyPS->OnPlayerRoleChanged.AddDynamic(this, &APlayerCharacter::BindToPartnerPlayerState);

		// 혹시 이미 데이터가 와 있을 수도 있으니 한번 체크
		TryApplyCustomization();

		// 혹시 이미 파트너가 정해져 있을 수도 있으니 체크
		if (MyPS->ConnectedPlayerIndex != -1)
		{
			BindToPartnerPlayerState(MyPS->bIsLowerBody);
		}
	}
}

ABRPlayerState* APlayerCharacter::GetUpperBodyPlayerState() const
{
	// 현재 캐릭터의 PlayerState (보통 하체/Movement 담당이 소유)
	ABRPlayerState* MyPS = Cast<ABRPlayerState>(GetPlayerState());
	if (!MyPS) return nullptr;

	// 만약 내가 상체 역할이라면 -> 내 PS 반환
	if (!MyPS->bIsLowerBody) return MyPS;

	// 만약 내가 하체 역할이라면 -> 연결된 파트너(상체)의 PS를 찾아야 함
	if (MyPS->ConnectedPlayerIndex != -1)
	{
		AGameStateBase* GS = UGameplayStatics::GetGameState(this);
		if (GS && GS->PlayerArray.IsValidIndex(MyPS->ConnectedPlayerIndex))
		{
			return Cast<ABRPlayerState>(GS->PlayerArray[MyPS->ConnectedPlayerIndex]);
		}
	}
	return nullptr;
}

ABRPlayerState* APlayerCharacter::GetLowerBodyPlayerState() const
{
	// 현재 캐릭터의 PlayerState
	ABRPlayerState* MyPS = Cast<ABRPlayerState>(GetPlayerState());
	if (!MyPS) return nullptr;

	// 내가 하체라면 -> 내 PS 반환
	if (MyPS->bIsLowerBody) return MyPS;

	// 내가 상체라면 -> 파트너(하체) PS 찾기
	if (MyPS->ConnectedPlayerIndex != -1)
	{
		AGameStateBase* GS = UGameplayStatics::GetGameState(this);
		if (GS && GS->PlayerArray.IsValidIndex(MyPS->ConnectedPlayerIndex))
		{
			return Cast<ABRPlayerState>(GS->PlayerArray[MyPS->ConnectedPlayerIndex]);
		}
	}
	return nullptr;
}

void APlayerCharacter::TryApplyCustomization()
{
	// 이미 둘 다 적용 끝났으면 더 이상 연산하지 않음 (최적화)
	if (bUpperBodyApplied && bLowerBodyApplied) return;

	ABRPlayerState* UpperPS = GetUpperBodyPlayerState();
	ABRPlayerState* LowerPS = GetLowerBodyPlayerState();

	// --- 1. 상체 적용 ---
	// 아직 적용 안 됐고(false), 데이터가 존재하면(HeadID != 0) 적용
	if (!bUpperBodyApplied && UpperPS && UpperPS->CustomizationData.HeadID != 0)
	{
		ApplyMeshFromID(EArmorSlot::Head, UpperPS->CustomizationData.HeadID);
		ApplyMeshFromID(EArmorSlot::Chest, UpperPS->CustomizationData.ChestID);
		ApplyMeshFromID(EArmorSlot::Hands, UpperPS->CustomizationData.HandsID);

		bUpperBodyApplied = true; // 완료 마킹 (이후에는 다시 적용 안 함)
		LOG_PLAYER(Display, TEXT("Upper Body Customization Applied"));
	}

	// --- 2. 하체 적용 ---
	if (!bLowerBodyApplied && LowerPS && LowerPS->CustomizationData.LegsID != 0)
	{
		ApplyMeshFromID(EArmorSlot::Legs, LowerPS->CustomizationData.LegsID);
		ApplyMeshFromID(EArmorSlot::Feet, LowerPS->CustomizationData.FeetID);

		bLowerBodyApplied = true; // 완료 마킹
		LOG_PLAYER(Display, TEXT("Lower Body Customization Applied"));
	}
}

void APlayerCharacter::BindToPartnerPlayerState(bool bIsLowerBody)
{
	// 1. 이미 파트너에게 바인딩이 되어 있다면 중복 실행 방지
	if (bBoundToPartner)
	{
		return;
	}

	// 2. 내 PlayerState 가져오기
	ABRPlayerState* MyPS = Cast<ABRPlayerState>(GetPlayerState());

	// 아직 내 PS가 없거나, 파트너가 배정되지 않았다면(ConnectedPlayerIndex == -1) 중단
	if (!MyPS || MyPS->ConnectedPlayerIndex == -1)
	{
		return;
	}

	// 3. GameState를 통해 파트너의 PlayerState 찾기
	// (PlayerArray에는 서버에 접속한 모든 플레이어의 상태가 들어있음)
	AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS || !GS->PlayerArray.IsValidIndex(MyPS->ConnectedPlayerIndex))
	{
		// 인덱스는 있지만 아직 GameState 배열이 동기화 덜 된 경우일 수 있음 (매우 드묾)
		return;
	}

	// 4. 파트너 PlayerState 캐스팅 및 바인딩
	ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(GS->PlayerArray[MyPS->ConnectedPlayerIndex]);
	if (PartnerPS)
	{
		// [핵심] 파트너의 커마 정보가 바뀌거나 도착하면 -> TryApplyCustomization 실행해라!
		PartnerPS->OnCustomizationDataChanged.AddDynamic(this, &APlayerCharacter::TryApplyCustomization);

		// 바인딩 성공 플래그 설정
		bBoundToPartner = true;

		LOG_PLAYER(Display, TEXT("Bound to Partner PlayerState: %s"), *PartnerPS->GetPlayerName());

		// 5. 혹시 파트너 정보가 이미 도착해있을 수도 있으니, 즉시 적용 시도 한 번 해봄
		TryApplyCustomization();
	}
}

void APlayerCharacter::ApplyMeshFromID(EArmorSlot Slot, int32 MeshID)
{
	// 실제 구현: ID를 기반으로 DataTable이나 AssetManager에서 SkeletalMesh를 로드하여 SetSkeletalMesh 호출
	// 예시:
	// USkeletalMesh* NewMesh = GetMeshFromDataTable(Slot, MeshID);
	// switch(Slot) {
	//    case EArmorSlot::Head: HeadMesh->SetSkeletalMesh(NewMesh); break;
	//    ...
	// }

	// 디버깅용 로그
	LOG_PLAYER(Display, TEXT("Applied Mesh ID %d to Slot %d"), MeshID, (int32)Slot);
}