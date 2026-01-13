#include "UpperBodyPawn.h"
#include "PlayerCharacter.h"
#include "DropItem.h"
#include "InteractableInterface.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "BRAttackComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUpperBodyPawn, Log, All);
#define BODY_LOG(Verbosity, Format, ...) UE_LOG(LogUpperBodyPawn, Verbosity, TEXT("%s: ") Format, *GetName(), ##__VA_ARGS__)

AUpperBodyPawn::AUpperBodyPawn()
{
	// [중요] 틱 그룹 설정: 모든 물리/이동 계산이 끝난 후 카메라를 갱신해야 떨림이 없습니다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	FrontCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("FrontCameraBoom"));
	FrontCameraBoom->SetupAttachment(RootComponent);

	// =================================================================
	// [핵심 1] 절대 회전 사용 (Absolute Rotation)
	// 몸통(척추)이 애니메이션으로 흔들려도, 카메라는 마우스 방향을 굳건히 유지합니다.
	// =================================================================
	FrontCameraBoom->SetUsingAbsoluteRotation(true);

	FrontCameraBoom->bUsePawnControlRotation = true;
	FrontCameraBoom->bDoCollisionTest = false;
	FrontCameraBoom->TargetArmLength = 0.0f;

	FrontCameraBoom->bInheritPitch = true;
	FrontCameraBoom->bInheritYaw = true;
	FrontCameraBoom->bInheritRoll = false;

	// =================================================================
	// [핵심 2] 카메라 렉(Lag) 끄기
	// 공격 속도가 빠를 때 카메라가 뒤쳐져서 캐릭터가 화면 밖으로 나가는 현상을 방지합니다.
	// =================================================================
	FrontCameraBoom->bEnableCameraLag = false;
	FrontCameraBoom->bEnableCameraRotationLag = false;

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
}

void AUpperBodyPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. 부모(몸통) 찾기 및 초기화
	if (!ParentBodyCharacter)
	{
		ParentBodyCharacter = Cast<APlayerCharacter>(GetAttachParentActor());

		if (ParentBodyCharacter && Controller)
		{
			float CurrentBodyYaw = ParentBodyCharacter->GetActorRotation().Yaw;
			FRotator NewRotation = Controller->GetControlRotation();

			// [복구] 카메라 초기 위치는 엉덩이(등) 쪽이어야 하므로 180도 유지가 맞습니다.
			NewRotation.Yaw = CurrentBodyYaw + 180.0f;

			Controller->SetControlRotation(NewRotation);
			LastBodyYaw = CurrentBodyYaw;
			return;
		}
	}

	if (!ParentBodyCharacter || !Controller) return;

	// 2. 몸통 회전 동기화 (탱크가 회전하면 상체 카메라도 같이 회전)
	float CurrentBodyYaw = ParentBodyCharacter->GetActorRotation().Yaw;
	float DeltaYaw = CurrentBodyYaw - LastBodyYaw;

	if (!FMath::IsNearlyZero(DeltaYaw))
	{
		FRotator CurrentRot = Controller->GetControlRotation();
		CurrentRot.Yaw += DeltaYaw;
		Controller->SetControlRotation(CurrentRot);
	}

	LastBodyYaw = CurrentBodyYaw;

	// -----------------------------------------------------------------
	// [각도 제한 로직]
	// -----------------------------------------------------------------
	FRotator CurrentControlRot = Controller->GetControlRotation();

	// 3. 좌우(Yaw) 시야각 제한 (등 뒤 기준)
	// [복구] 기준점도 다시 +180(등 뒤)으로 잡습니다.
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
	// [진짜 해결책] 상체 시선 전달 (머리 방향 교정)
	// =================================================================
	if (ParentBodyCharacter)
	{
		// 문제 원인: 카메라는 180도(등 뒤)를 보고 있는데, 그 값을 그대로 머리에 주니까
		// 머리도 180도 뒤를 봐서 "엑소시스트"가 되는 것입니다.

		// 해결: 머리(애니메이션)에 전달할 때만 180도를 다시 뒤집어서 "앞"을 보게 만듭니다.
		FRotator TargetHeadRot = Controller->GetControlRotation();
		TargetHeadRot.Yaw += 180.0f;

		ParentBodyCharacter->SetUpperBodyRotation(TargetHeadRot);
	}
}

void AUpperBodyPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
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

void AUpperBodyPawn::ServerRequestSetAttackDetection_Implementation(bool bEnabled)
{
	// 여기는 이제 서버입니다. 
	// 서버에서 안전하게 하체 캐릭터의 컴포넌트 로직을 실행합니다.
	if (ParentBodyCharacter && ParentBodyCharacter->AttackComponent)
	{
		// 서버이므로 일반 SetAttackDetection 호출 (RPC 아님)
		ParentBodyCharacter->AttackComponent->SetAttackDetection(bEnabled);
	}
}

void AUpperBodyPawn::Attack(const FInputActionValue& Value)
{
	if (bIsAttacking || !ParentBodyCharacter) return;

	bIsAttacking = true;
	ParentBodyCharacter->TriggerUpperBodyAttack();

	// [수정] 컴포넌트의 RPC가 아닌, 자신의 서버 RPC 호출
	ServerRequestSetAttackDetection(true);
}

void AUpperBodyPawn::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bIsAttacking = false;

	// 종료 시에도 자신의 서버 RPC 호출
	ServerRequestSetAttackDetection(false);
}

void AUpperBodyPawn::Interact(const FInputActionValue& Value)
{
	// =================================================================
	// [테스트 1] 입력 확인용 메시지 출력
	// =================================================================
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("E Key Pressed!"));
	}

	// 1. 부모(몸통) 캐릭터 확인
	if (!ParentBodyCharacter)
	{
		ParentBodyCharacter = Cast<APlayerCharacter>(GetAttachParentActor());
		if (!ParentBodyCharacter) return;
	}

	// =================================================================
	// [수정됨] 레이캐스트 시작점: 카메라 -> 머리 메쉬의 'head' 소켓 위치
	// =================================================================
	FVector Start;

	// 'head'라는 뼈(소켓)가 존재하면 그 위치를 사용
	if (ParentBodyCharacter->GetMesh() && ParentBodyCharacter->GetMesh()->DoesSocketExist(TEXT("head")))
	{
		Start = ParentBodyCharacter->HeadMountPoint->GetComponentLocation();
	}
	else
	{
		// 만약 소켓을 못 찾으면 안전하게 카메라 위치 사용 (혹은 로그 출력)
		Start = FrontCamera->GetComponentLocation();
		BODY_LOG(Warning, TEXT("Cannot find 'head' socket on HeadMesh. Using Camera location instead."));
	}

	// 방향: 카메라는 계속 정면을 보고 있으므로, 카메라의 Forward Vector를 사용합니다.
	FVector End = Start + (FrontCamera->GetForwardVector() * InteractionDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(ParentBodyCharacter);

	// 3. 레이캐스트 발사 (무기와 아이템 모두 Visibility 채널 Block 설정이 되어있어야 함)
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	// =================================================================
	// [테스트 2] 레이캐스트 시각화
	// =================================================================
	if (bHit)
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 2.0f, 0, 1.0f);
		DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 10.0f, 12, FColor::Green, false, 2.0f);
	}
	else
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 2.0f, 0, 1.0f);
	}

	// 4. 아이템/무기 확인 및 상호작용
	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();

		// 이를 통해 무기(BaseWeapon)와 아이템(DropItem) 모두 상호작용 가능해짐
		if (IInteractableInterface* Interface = Cast<IInteractableInterface>(HitActor))
		{
			// 화면에 감지된 대상 이름 출력
			if (GEngine)
			{
				FString Msg = FString::Printf(TEXT("Interactable Detected: %s"), *HitActor->GetName());
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, Msg);
			}

			BODY_LOG(Log, TEXT("Interacting with %s"), *HitActor->GetName());

			// 인터페이스 함수 호출 (무기 장착 or 아이템 획득)
			Interface->Interact(ParentBodyCharacter);
		}
	}
}