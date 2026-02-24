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
#include "BRGameInstance.h"
#include "BRPlayerState.h"
#include "BRGameState.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogPlayerChar);

#define LOG_PLAYER(Verbosity, Format, ...) \
    UE_LOG(LogPlayerChar, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(Format, ##__VA_ARGS__))

APlayerCharacter::APlayerCharacter()
{
	// [기본 설정 유지 및 수정]
	bUseControllerRotationPitch = false;

	// 1. 마우스 시점 이동 관성 (회전)
	// 컨트롤러의 방향으로 즉각 회전하지 않고 부드럽게 돌아가도록 변경합니다.
	bUseControllerRotationYaw = false; // 기존 true에서 false로 변경하여 즉각 회전을 방지합니다.
	bUseControllerRotationRoll = false;

	// 회전 관성 적용
	GetCharacterMovement()->bUseControllerDesiredRotation = true; // 컨트롤러 시점 방향으로 천천히 회전하게 만듭니다.
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 300.0f, 0.0f); // Yaw 수치가 낮을수록 회전이 더 묵직하고 느려집니다. (기존 500.0f)
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// 2. WASD 이동 관성 (가속 및 감속)
	// 캐릭터의 초기 가속을 느리게 하고, 키를 뗐을 때 즉시 멈추지 않고 미끄러지듯 감속하게 합니다.
	GetCharacterMovement()->MaxAcceleration = 600.f; // 가속도: 수치가 낮을수록 최고 속도에 도달하기까지 오래 걸려 무겁게 느껴집니다.
	GetCharacterMovement()->bUseSeparateBrakingFriction = true; // 감속 마찰력을 별도로 사용하도록 활성화합니다.
	GetCharacterMovement()->BrakingFriction = 1.f; // 마찰력: 수치가 낮을수록 키를 뗐을 때 지면에서 더 많이 미끄러집니다.
	GetCharacterMovement()->BrakingDecelerationWalking = 500.f; // 감속도: 수치가 낮을수록 완전히 정지할 때까지의 거리가 길어집니다.

	// GetMesh()->SetOwnerNoSee(true); // <- 몸 투명화
	GetMesh()->bCastHiddenShadow = true;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	TArray<USkeletalMeshComponent*> ArmorParts = { HeadMesh, ChestMesh, HandMesh, LegMesh, FootMesh };

	for (USkeletalMeshComponent* Part : ArmorParts)
	{
		if (Part)
		{
			// Part->SetOwnerNoSee(true);   //  <- 몸 투명화
			Part->bCastHiddenShadow = true;

			// A. [틱 순서 고정] 
			Part->AddTickPrerequisiteComponent(GetMesh());

			// B. [부착 관계 확인]
			if (Part->GetAttachParent() != GetMesh())
			{
				Part->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale);

				// UE_LOG(LogTemp, Warning, TEXT("[%s] 파츠가 GetMesh에 붙어있지 않아 강제로 재부착했습니다."), *Part->GetName());
			}

			// C. [Leader Pose 재확인]
			Part->SetLeaderPoseComponent(GetMesh());
		}
	}

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

	// 3. 스태미나 컴포넌트 생성
	StaminaComp = CreateDefaultSubobject<UStaminaComponent>(TEXT("StaminaComp"));

	// [네트워크] 클라이언트 예측/서버 보정 및 스무딩 튜닝 (고지연·패킷유실 환경 대응)
	bReplicates = true;
	SetNetUpdateFrequency(144.0f);
	SetMinNetUpdateFrequency(100.0f);

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
	// 고지연(100ms+) 환경: 보정 허용 거리 확대 → 불필요한 보정·덜컹임 감소
	MoveComp->NetworkMaxSmoothUpdateDistance = 256.0f;   // 이 거리 이하만 스무딩, 그 이상은 보정 허용
	MoveComp->NetworkNoSmoothUpdateDistance = 0.0f;    // 0 = 작은 오차도 스무딩으로 흡수
	// 서버-클라이언트 위치 오차가 이 값(단위: cm) 이하면 보정 생략 → 핑 높을 때 덜 튐
	MoveComp->NetworkLargeClientCorrectionDistance = 120.0f;
}

void APlayerCharacter::BeginPlay()
{
	//GetMesh()->SetVisibility(false, false); //  <- 몸 투명화

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

	ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>();

	if (BRPS)
	{
		// 2. 이미 PlayerState가 있다면 초기화 로직 실행
		// (OnRep_PlayerState 내부에서 TryApplyCustomization 바인딩 및 호출을 수행하도록 구성했다면 이 함수 호출만으로 충분합니다)
		OnRep_PlayerState();
	}
	else
	{
		// 3. PlayerState가 아직 없다면(클라이언트 로딩 지연 등), 0.5초 뒤 재시도
		FTimerHandle RetryHandle;
		GetWorld()->GetTimerManager().SetTimer(RetryHandle, [this]()
			{
				// 람다 내부에서 다시 확인
				if (ABRPlayerState* RetryPS = GetPlayerState<ABRPlayerState>())
				{
					// 재시도 성공 시 초기화 실행
					// OnRep_PlayerState를 수동으로 호출하여 바인딩/적용 로직을 수행
					OnRep_PlayerState();

					UE_LOG(LogTemp, Log, TEXT("[Character] BeginPlay: PlayerState 뒤늦게 로드됨 -> 초기화 수행"));
				}
			}, 0.5f, false);
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
		// 파트너 바인딩은 로직 유지를 위해 연결
		MyPS->OnPlayerRoleChanged.AddDynamic(this, &APlayerCharacter::BindToPartnerPlayerState);

		// 적용 시도
		TryApplyCustomization();

		// 파트너가 이미 지정되어 있다면 바인딩 시도
		if (MyPS->ConnectedPlayerIndex != -1)
		{
			BindToPartnerPlayerState(MyPS->bIsLowerBody);
		}
	}

	UpdateHPUI();
}

// 상체 플레이어 찾기 (포인터 우선)
ABRPlayerState* APlayerCharacter::GetUpperBodyPlayerState() const
{
	ABRPlayerState* MyPS = Cast<ABRPlayerState>(GetPlayerState());
	if (!MyPS) return nullptr;

	// 1. 내가 상체면 -> 나 자신 리턴
	if (!MyPS->bIsLowerBody) return MyPS;

	// 2. [핵심] 내가 하체면 -> PartnerPlayerState 포인터 확인 (가장 확실함)
	if (MyPS->PartnerPlayerState)
	{
		return MyPS->PartnerPlayerState;
	}
	return nullptr;
}

// 하체 플레이어 찾기 (포인터 우선)
ABRPlayerState* APlayerCharacter::GetLowerBodyPlayerState() const
{
	ABRPlayerState* MyPS = Cast<ABRPlayerState>(GetPlayerState());
	if (!MyPS) return nullptr;

	// 1. 내가 하체면 -> 나 자신 리턴
	if (MyPS->bIsLowerBody) return MyPS;

	// 2. [핵심] 내가 상체면 -> PartnerPlayerState 포인터 확인
	if (MyPS->PartnerPlayerState)
	{
		return MyPS->PartnerPlayerState;
	}

	return nullptr;
}

void APlayerCharacter::TryApplyCustomization()
{
	// [핵심 1] 이미 최초 외형 세팅이 끝나서 잠겼다면, SwitchOrb 스왑 등으로 불려도 무시합니다.
	if (bAppearanceLocked) return;

	ABRPlayerState* MyPS = Cast<ABRPlayerState>(GetPlayerState());
	if (!MyPS) return;

	// [역할 동기화 과도기 방지 가드]
	if (MyPS->PartnerPlayerState && (MyPS->bIsLowerBody == MyPS->PartnerPlayerState->bIsLowerBody))
	{
		return;
	}

	ABRPlayerState* UpperPS = GetUpperBodyPlayerState();
	ABRPlayerState* LowerPS = GetLowerBodyPlayerState();

	// --- 상체 적용 ---
	if (UpperPS && !UpperPS->bIsLowerBody)
	{
		int32 ApplyHeadID = UpperPS->CustomizationData.bIsDataValid ? UpperPS->CustomizationData.HeadID : 0;
		int32 ApplyChestID = UpperPS->CustomizationData.bIsDataValid ? UpperPS->CustomizationData.ChestID : 0;
		int32 ApplyHandID = UpperPS->CustomizationData.bIsDataValid ? UpperPS->CustomizationData.HandID : 0;

		ApplyMeshFromID(EArmorSlot::Head, ApplyHeadID);
		ApplyMeshFromID(EArmorSlot::Chest, ApplyChestID);
		ApplyMeshFromID(EArmorSlot::Hands, ApplyHandID);
	}

	// --- 하체 적용 ---
	if (LowerPS && LowerPS->bIsLowerBody)
	{
		int32 ApplyLegID = LowerPS->CustomizationData.bIsDataValid ? LowerPS->CustomizationData.LegID : 0;
		int32 ApplyFootID = LowerPS->CustomizationData.bIsDataValid ? LowerPS->CustomizationData.FootID : 0;

		ApplyMeshFromID(EArmorSlot::Legs, ApplyLegID);
		ApplyMeshFromID(EArmorSlot::Feet, ApplyFootID);
	}

	// =================================================================
	// [핵심 2] 두 플레이어의 상/하체 역할이 정상적으로 나뉘어 배정되었다면,
	// 커마 적용이 완전하게 끝났다고 판단하고 영구 잠금(Lock)을 겁니다.
	// =================================================================
	if (MyPS && MyPS->PartnerPlayerState && (MyPS->bIsLowerBody != MyPS->PartnerPlayerState->bIsLowerBody))
	{
		bAppearanceLocked = true;
		LOG_PLAYER(Display, TEXT("Initial Appearance Fully Locked. It won't change on SwitchOrb Swaps."));
	}
}

void APlayerCharacter::BindToPartnerPlayerState(bool bIsLowerBody)
{
	ABRPlayerState* MyPS = Cast<ABRPlayerState>(GetPlayerState());
	if (!MyPS)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerDelegate RetryDelegate = FTimerDelegate::CreateUObject(this, &APlayerCharacter::BindToPartnerPlayerState, bIsLowerBody);
			World->GetTimerManager().SetTimer(TimerHandle_RetryBindPartner, RetryDelegate, 0.5f, false);
		}
		return;
	}

	// 1순위: 포인터 확인 (가장 정확함)
	ABRPlayerState* PartnerPS = MyPS->PartnerPlayerState;

	// 포인터가 아직 안 왔는데, 연결된 인덱스는 있다? -> 로딩 중이니 재시도
	if (!PartnerPS && MyPS->ConnectedPlayerIndex != -1)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerDelegate RetryDelegate = FTimerDelegate::CreateUObject(this, &APlayerCharacter::BindToPartnerPlayerState, bIsLowerBody);
			World->GetTimerManager().SetTimer(TimerHandle_RetryBindPartner, RetryDelegate, 0.5f, false);
		}
		return;
	}

	// 파트너가 아예 없는 경우 (솔로 등)
	if (!PartnerPS) return;

	// --- 성공 ---
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_RetryBindPartner);

	// [핵심] SwitchOrb 등으로 파트너 역할이 스왑될 수 있으므로, 항상 기존 바인딩을 제거하고 새로 연결
	PartnerPS->OnCustomizationDataChanged.RemoveDynamic(this, &APlayerCharacter::TryApplyCustomization);
	PartnerPS->OnCustomizationDataChanged.AddDynamic(this, &APlayerCharacter::TryApplyCustomization);

	PartnerPS->OnPlayerRoleChanged.RemoveDynamic(this, &APlayerCharacter::BindToPartnerPlayerState);
	PartnerPS->OnPlayerRoleChanged.AddDynamic(this, &APlayerCharacter::BindToPartnerPlayerState);

	LOG_PLAYER(Display, TEXT("Bound to Partner Success: %s"), *PartnerPS->GetPlayerName());

	// 파트너가 갱신되었으므로 커스터마이징 갱신을 찔러줍니다.
	// (최초 1회는 정상 적용 후 bAppearanceLocked가 true가 되며, 이후 스왑 시에는 무시되어 외형이 고정됩니다.)
	TryApplyCustomization();
}

void APlayerCharacter::ApplyMeshFromID(EArmorSlot Slot, int32 MeshID)
{
	// 1. GameInstance 가져오기
	UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance());
	if (!GI)
	{
		// 에디터 등에서 PIE 시작 전이거나 엣지 케이스
		return;
	}

	// 2. GameInstance의 맵에서 'ArmorData' 테이블 찾기
	UDataTable* ArmorDT = nullptr;
	if (GI->ConfigDataMap.Contains(TEXT("ArmorData")))
	{
		ArmorDT = GI->ConfigDataMap[TEXT("ArmorData")];
	}

	// 테이블이 없으면 중단
	if (!ArmorDT)
	{
		LOG_PLAYER(Error, TEXT("ArmorDataTable Not Found in GameInstance ConfigMap!"));
		return;
	}

	// 3. 타겟 컴포넌트 선정
	USkeletalMeshComponent* TargetMeshComp = nullptr;
	USkeletalMesh* MeshToApply = nullptr; // 적용할 메시를 담을 변수

	switch (Slot)
	{
	case EArmorSlot::Head:
		TargetMeshComp = HeadMesh;
		MeshToApply = DefaultHeadMesh; // 기본값 설정
		break;
	case EArmorSlot::Chest:
		TargetMeshComp = ChestMesh;
		MeshToApply = DefaultChestMesh;
		break;
	case EArmorSlot::Hands:
		TargetMeshComp = HandMesh;
		MeshToApply = DefaultHandMesh;
		break;
	case EArmorSlot::Legs:
		TargetMeshComp = LegMesh;
		MeshToApply = DefaultLegMesh;
		break;
	case EArmorSlot::Feet:
		TargetMeshComp = FootMesh;
		MeshToApply = DefaultFootMesh;
		break;
	}

	if (!TargetMeshComp) return;

	// 4. ID가 0인 경우 (장비 해제) -> 기본(맨몸) 메시 적용
	if (MeshID == 0)
	{
		TargetMeshComp->SetSkeletalMesh(MeshToApply);
		LOG_PLAYER(Display, TEXT("Applied Default Mesh for Slot %d"), (int32)Slot);
		return;
	}

	// 5. 데이터 검색
	FArmorData* FoundData = nullptr;

	static const FString ContextString(TEXT("ApplyMeshFromID_Search"));
	TArray<FArmorData*> AllRows;

	// 테이블의 모든 행을 가져옵니다.
	ArmorDT->GetAllRows<FArmorData>(ContextString, AllRows);

	// 반복문으로 하나씩 ID를 대조합니다.
	for (FArmorData* Row : AllRows)
	{
		if (Row && Row->ID == MeshID)
		{
			FoundData = Row;
			break;
		}
	}

	// 6. 적용
	if (FoundData && FoundData->ArmorMesh)
	{
		TargetMeshComp->SetSkeletalMesh(FoundData->ArmorMesh);
		LOG_PLAYER(Display, TEXT("Applied Mesh ID %d via GameInstance"), MeshID);
	}
	else
	{
		// ID는 있는데 데이터를 못 찾았다면 안전하게 기본 메시 적용
		TargetMeshComp->SetSkeletalMesh(MeshToApply);
	}
}

void APlayerCharacter::UpdatePreviewMesh(const FBRCustomizationData& NewData)
{
	// 각 부위별로 ApplyMeshFromID를 직접 호출 (기존 함수 재활용)
	ApplyMeshFromID(EArmorSlot::Head, NewData.HeadID);
	ApplyMeshFromID(EArmorSlot::Chest, NewData.ChestID);
	ApplyMeshFromID(EArmorSlot::Hands, NewData.HandID);
	ApplyMeshFromID(EArmorSlot::Legs, NewData.LegID);
	ApplyMeshFromID(EArmorSlot::Feet, NewData.FootID);

	// 로그 확인용
	// LOG_PLAYER(Display, TEXT("Preview Updated: HeadID %d"), NewData.HeadID);
}

void APlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// [크래시 방지] 레벨 이동 시 물리 엔진이 소멸된 컴포넌트를 참조하지 않도록 강제 종료
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetSimulatePhysics(false);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (GetMesh())
	{
		GetMesh()->SetSimulatePhysics(false);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 파츠들도 물리 끄기
	TArray<USkeletalMeshComponent*> Parts = { HeadMesh, ChestMesh, HandMesh, LegMesh, FootMesh };
	for (USkeletalMeshComponent* Part : Parts)
	{
		if (Part)
		{
			Part->SetSimulatePhysics(false);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// [핵심] 캐릭터가 파괴될 때 돌고 있던 타이머를 모두 해제하여 
	// 댕글링 포인터 크래시(Access Violation) 방지
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_RetryBindPartner);
	}
}