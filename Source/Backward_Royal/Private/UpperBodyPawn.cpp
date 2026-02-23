#include "UpperBodyPawn.h"
#include "PlayerCharacter.h"
#include "DropItem.h"
#include "BaseWeapon.h"
#include "InteractableInterface.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "BRAttackComponent.h"
#include "BRPlayerController.h"
#include "Engine/OverlapResult.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUpperBodyPawn, Log, All);
#define BODY_LOG(Verbosity, Format, ...) UE_LOG(LogUpperBodyPawn, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

AUpperBodyPawn::AUpperBodyPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	// 서버에서 스폰된 상체가 모든 클라이언트에 보이도록 복제 명시 (미설정 시 하체만 4명 보이는 현상 방지)
	bReplicates = true;
	SetReplicateMovement(true);
	bOnlyRelevantToOwner = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	FrontCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("FrontCameraBoom"));
	FrontCameraBoom->SetupAttachment(RootComponent);

	FrontCameraBoom->SetUsingAbsoluteRotation(true);
	FrontCameraBoom->bUsePawnControlRotation = true;
	FrontCameraBoom->bDoCollisionTest = false;
	FrontCameraBoom->TargetArmLength = 0.0f;

	FrontCameraBoom->bInheritPitch = true;
	FrontCameraBoom->bInheritYaw = true;
	FrontCameraBoom->bInheritRoll = false;

	FrontCameraBoom->bEnableCameraLag = true;           // 이동 지연 켜기
	FrontCameraBoom->CameraLagSpeed = 20.0f;            // 이동 지연 속도
	FrontCameraBoom->bEnableCameraRotationLag = true;   // 회전 지연 켜기 (끌려갈 때 부드럽게)
	FrontCameraBoom->CameraRotationLagSpeed = 15.0f;    // 수치가 작을수록 더 부드럽고 늦게 따라감

	FrontCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FrontCamera"));
	FrontCamera->SetupAttachment(FrontCameraBoom);
	FrontCamera->bUsePawnControlRotation = false;

	LastBodyYaw = 0.0f;
	ParentBodyCharacter = nullptr;
	InteractionDistance = 300.0f;
}

void AUpperBodyPawn::BeginPlay()
{
	Super::BeginPlay();

	if (FrontCameraBoom)
	{
		FrontCameraBoom->AddTickPrerequisiteActor(this);
	}

	// 입력 매핑 등록
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (UpperBodyMappingContext)
			{
				Subsystem->AddMappingContext(UpperBodyMappingContext, 0);
			}
		}
	}

	//if (GEngine)
	//{
	//	// 1. 현재 네트워크 상태 확인
	//	FString NetRole = TEXT("None");
	//	switch (GetLocalRole())
	//	{
	//	case ROLE_Authority: NetRole = TEXT("Authority (Server)"); break;
	//	case ROLE_AutonomousProxy: NetRole = TEXT("AutonomousProxy (Client)"); break;
	//	case ROLE_SimulatedProxy: NetRole = TEXT("SimulatedProxy (Other)"); break;
	//	}

	//	// 2. 소유자 및 컨트롤러 확인 (이제 안전함)
	//	FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("No Owner");
	//	FString ControllerName = GetController() ? GetController()->GetName() : TEXT("No Controller");

	//	// 3. 화면 출력
	//	FString DebugMsg = FString::Printf(TEXT("[%s] Pawn: %s | Owner: %s | Controller: %s"),
	//		*NetRole, *GetName(), *OwnerName, *ControllerName);

	//	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, DebugMsg);
	//}
}

void AUpperBodyPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// [크래시 방지] 물리/충돌 컴포넌트 정리
	// 상체 폰은 RootComponent가 SceneComponent일 수 있으나, 
	// 혹시 모를 물리 충돌이나 자식 컴포넌트의 물리 작용을 차단
	if (RootComponent)
	{
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(RootComponent))
		{
			PrimComp->SetSimulatePhysics(false);
			PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void AUpperBodyPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. 부모 몸통 찾기
	if (!ParentBodyCharacter)
	{
		ParentBodyCharacter = Cast<APlayerCharacter>(GetAttachParentActor());

		if (ParentBodyCharacter && Controller)
		{
			float CurrentBodyYaw = ParentBodyCharacter->GetActorRotation().Yaw;
			FRotator NewRotation = Controller->GetControlRotation();

			// [초기화] 등 뒤(180도)를 바라보도록 설정
			NewRotation.Yaw = CurrentBodyYaw + 180.0f;

			Controller->SetControlRotation(NewRotation);
			LastBodyYaw = CurrentBodyYaw;
		}
	}

	if (!ParentBodyCharacter || !Controller) return;

	// 2. 몸통 회전 동기화 (탱크가 회전하면 상체 카메라도 같이 회전)
	// [수정] 하체가 회전해도 상체 카메라는 고정되어야(상하체 분리) 하므로 동기화 코드를 주석 처리합니다.
	/*
	float CurrentBodyYaw = ParentBodyCharacter->GetActorRotation().Yaw;
	float DeltaYaw = CurrentBodyYaw - LastBodyYaw;

	if (!FMath::IsNearlyZero(DeltaYaw))
	{
		FRotator CurrentRot = Controller->GetControlRotation();
		CurrentRot.Yaw += DeltaYaw;
		Controller->SetControlRotation(CurrentRot);
	}
	*/

	// 동기화는 안 해도 변수 업데이트는 해둡니다.
	float CurrentBodyYaw = ParentBodyCharacter->GetActorRotation().Yaw;
	LastBodyYaw = CurrentBodyYaw;

	// -----------------------------------------------------------------
	// [각도 제한 로직]
	// -----------------------------------------------------------------
	FRotator CurrentControlRot = Controller->GetControlRotation();

	// 3. 좌우(Yaw) 시야각 제한 (등 뒤 기준)
	// 기준점도 +180(등 뒤)으로 잡습니다.
	float BodyFrontYaw = ParentBodyCharacter->GetActorRotation().Yaw + 180.0f;
	float RelativeYaw = FRotator::NormalizeAxis(CurrentControlRot.Yaw - BodyFrontYaw);
	float ClampedYaw = FMath::Clamp(RelativeYaw, -90.0f, 90.0f);

	// 4. 위아래(Pitch) 시야각 제한
	float CurrentPitch = FRotator::NormalizeAxis(CurrentControlRot.Pitch);
	float ClampedPitch = FMath::Clamp(CurrentPitch, -90.0f, 90.0f);

	// 5. 제한된 각도 적용
	bool bYawChanged = !FMath::IsNearlyEqual(RelativeYaw, ClampedYaw);
	bool bPitchChanged = !FMath::IsNearlyEqual(CurrentPitch, ClampedPitch);

	if (bYawChanged || bPitchChanged)
	{
		CurrentControlRot.Yaw = BodyFrontYaw + ClampedYaw;
		CurrentControlRot.Pitch = ClampedPitch;
		Controller->SetControlRotation(CurrentControlRot);
	}

	// =================================================================
	if (ParentBodyCharacter)
	{
		// [유지] 머리(애니메이션)에 전달할 때는 180도를 더해서 정면을 보게 만듭니다.
		FRotator TargetHeadRot = Controller->GetControlRotation();
		TargetHeadRot.Yaw += 180.0f;

		ParentBodyCharacter->SetUpperBodyRotation(TargetHeadRot);

		if (IsLocallyControlled() && !HasAuthority())
		{
			ServerUpdateAimRotation(TargetHeadRot);
		}
	}
}

void AUpperBodyPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (IsLocallyControlled())
	{
		// 1. 입력 컴포넌트 재설정 (조종기 역할 복구)
		if (InputComponent)
		{
			SetupPlayerInputComponent(InputComponent);
			UE_LOG(LogTemp, Warning, TEXT("상체 역할 복귀: 입력 바인딩 재설정 완료"));
		}

		// 2. 상체용 IMC(입력 매핑) 강제 로드
		if (ABRPlayerController* PC = Cast<ABRPlayerController>(GetController()))
		{
			PC->SetupRoleInput(false); // 상체는 false
			PC->SetIgnoreMoveInput(true); // 상체 조종 중에는 하체 이동 입력 무시(필요시)
			PC->SetViewTarget(this);
		}
	}
}

void AUpperBodyPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (!PlayerInputComponent) return;

	// 기존 바인딩 제거 (중요)
	PlayerInputComponent->ClearActionBindings();

	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (LookAction)
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AUpperBodyPawn::Look);

		if (AttackAction)
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AUpperBodyPawn::Attack);

		if (InteractAction)
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AUpperBodyPawn::Interact);
	}
}

void AUpperBodyPawn::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y * -1.0f); // 마우스 Y축 반전 (위로 올리면 위를 봄)

}

void AUpperBodyPawn::Attack(const FInputActionValue& Value)
{
	// 본인이 로컬에서 컨트롤 중인 Pawn이 아니면 무시
	if (!IsLocallyControlled() || !ParentBodyCharacter) return;

	ServerRequestSetAttackDetection(true);
}

void AUpperBodyPawn::ServerRequestSetAttackDetection_Implementation(bool bEnabled)
{
	//FString NetMode = HasAuthority() ? TEXT("Server") : TEXT("Client");
	//GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan,
	//	FString::Printf(TEXT("[%s] Collision Enabled (Pawn: %s)"), *NetMode, *GetName()));

	if (!ParentBodyCharacter) return;

	if (bEnabled)
	{
		ParentBodyCharacter->RequestAttack();
		//// 서버가 "하체 캐릭터"에게 멀티캐스트 재생을 명령합니다.
		//// 하체는 모든 플레이어가 공유하므로 서버 월드에서도 무기가 움직입니다.
		//ParentBodyCharacter->MulticastPlayAttack(this);

		//if (ParentBodyCharacter->AttackComponent)
		//{
		//	ParentBodyCharacter->AttackComponent->SetAttackDetection(true); // 서버 물리 판정 ON
		//}
	}
}

void AUpperBodyPawn::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (HasAuthority()) // 서버에서만 판정을 종료합니다.
	{
		if (ParentBodyCharacter && ParentBodyCharacter->AttackComponent)
		{
			ParentBodyCharacter->AttackComponent->SetAttackDetection(false);
		}
	}
}

void AUpperBodyPawn::Interact(const FInputActionValue& Value)
{
	if (!ParentBodyCharacter)
	{
		ParentBodyCharacter = Cast<APlayerCharacter>(GetAttachParentActor());
		if (!ParentBodyCharacter) return;
	}

	// 1. 탐색 시작점 설정 (카메라 위치 혹은 캐릭터 위치)
	FVector SearchOrigin = FrontCamera->GetComponentLocation();

	// 2. 주변의 모든 액터 검출 (Overlap)
	TArray<FOverlapResult> OverlapResults;
	FCollisionShape SearchSphere = FCollisionShape::MakeSphere(InteractionDistance);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(ParentBodyCharacter);

	if (ParentBodyCharacter->CurrentWeapon)
	{
		QueryParams.AddIgnoredActor(ParentBodyCharacter->CurrentWeapon);
	}

	// ECC_Visibility 채널을 사용하여 주변 액터 탐색
	bool bHasOverlap = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		SearchOrigin,
		FQuat::Identity,
		ECC_Visibility,
		SearchSphere,
		QueryParams
	);

	if (bHasOverlap)
	{
		AActor* ClosestActor = nullptr;
		float MinDistance = InteractionDistance + 100.0f; // 초기값 설정

		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* PotentialActor = Result.GetActor();
			if (!PotentialActor) continue;

			// 상호작용 인터페이스 구현 여부 확인
			if (PotentialActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
			{
				float DistanceToActor = FVector::Dist(SearchOrigin, PotentialActor->GetActorLocation());

				// 가장 가까운 액터 갱신
				if (DistanceToActor < MinDistance)
				{
					MinDistance = DistanceToActor;
					ClosestActor = PotentialActor;
				}
			}
		}

		// 3. 가장 가까운 액터를 찾았다면 서버에 상호작용 요청
		if (ClosestActor)
		{
			ServerRequestInteract(ClosestActor);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
					FString::Printf(TEXT("Closest Interactable Found: %s"), *ClosestActor->GetName()));
			}

			// 시각적 피드백 (디버그용)
			DrawDebugSphere(GetWorld(), ClosestActor->GetActorLocation(), 30.0f, 12, FColor::Green, false, 2.0f);
		}
	}
	else
	{
		// 아무것도 찾지 못했을 때 디버그 표시
		DrawDebugSphere(GetWorld(), SearchOrigin, InteractionDistance, 12, FColor::Red, false, 1.0f);
	}
}

void AUpperBodyPawn::ServerRequestInteract_Implementation(AActor* TargetActor)
{
	if (TargetActor && ParentBodyCharacter)
	{
		// 2. 서버 월드에서 인터페이스 실행
		// 서버는 모든 권한을 가지고 있으므로 무기 장착이든 아이템 획득이든 즉시 성공합니다.
		if (IInteractableInterface* Interface = Cast<IInteractableInterface>(TargetActor))
		{
			Interface->Interact(ParentBodyCharacter);
			BODY_LOG(Log, TEXT("Server: %s가 %s와 상호작용 수행 완료"), *ParentBodyCharacter->GetName(), *TargetActor->GetName());
		}
	}
}

void AUpperBodyPawn::ServerUpdateAimRotation_Implementation(FRotator NewRotation)
{
	if (ParentBodyCharacter)
	{
		// 2. 부모가 있어서 업데이트 성공
		ParentBodyCharacter->SetUpperBodyRotation(NewRotation);

		//FString Msg = FString::Printf(TEXT("[Server] Update Success! Angle: %f"), NewRotation.Yaw);
		//GEngine->AddOnScreenDebugMessage(502, 1.f, FColor::Green, Msg);
	}
	else
	{
		// 3. [문제 발생] 부모를 못 찾음 -> 여기서 막히고 있을 확률 99%
		GEngine->AddOnScreenDebugMessage(502, 1.f, FColor::Red, TEXT("[Server] ERROR: ParentBodyCharacter is NULL!"));

		// 강제로 다시 찾기 시도
		ParentBodyCharacter = Cast<APlayerCharacter>(GetAttachParentActor());
		if (ParentBodyCharacter)
		{
			ParentBodyCharacter->SetUpperBodyRotation(NewRotation);
			GEngine->AddOnScreenDebugMessage(503, 1.f, FColor::Cyan, TEXT("[Server] Recovered & Updated!"));
		}
	}
}