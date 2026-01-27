#include "SoloTesterCharacter.h"
#include "UpperBodyPawn.h" 
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h" 
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"

// [필수 헤더]
#include "Camera/CameraComponent.h"
#include "InteractableInterface.h"
#include "DrawDebugHelpers.h"

ASoloTesterCharacter::ASoloTesterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = true;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
		GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
		GetCharacterMovement()->MaxWalkSpeed = 400.0f;
	}

	if (RearCameraBoom)
	{
		RearCameraBoom->bUsePawnControlRotation = true;
	}
}

void ASoloTesterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetMesh()) GetMesh()->SetOwnerNoSee(false);

	if (UpperBodyClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();

		UpperBodyInstance = GetWorld()->SpawnActor<AUpperBodyPawn>(UpperBodyClass, GetActorTransform(), SpawnParams);

		if (UpperBodyInstance)
		{
			if (HeadMountPoint)
				UpperBodyInstance->AttachToComponent(HeadMountPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			else
				UpperBodyInstance->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

			UpperBodyInstance->SetOwner(this);
		}
	}
}

void ASoloTesterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (TestAttackAction)
		{
			EnhancedInputComponent->BindAction(TestAttackAction, ETriggerEvent::Started, this, &ASoloTesterCharacter::RelayAttack);
		}
		if (TestInteractAction)
		{
			EnhancedInputComponent->BindAction(TestInteractAction, ETriggerEvent::Started, this, &ASoloTesterCharacter::RelayInteract);
		}
	}
}

void ASoloTesterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (UpperBodyInstance && GetController())
	{
		UpperBodyInstance->SetActorRotation(GetControlRotation());
	}
	UpperBodyAimRotation = GetControlRotation();
}

void ASoloTesterCharacter::RelayAttack(const FInputActionValue& Value)
{
	// 메인 캐릭터(부모)의 공격 함수 호출 (자동으로 왼손/오른손/무기 판단)
	RequestAttack();
}

void ASoloTesterCharacter::ForceAttack()
{
	RequestAttack();
}

void ASoloTesterCharacter::RelayInteract(const FInputActionValue& Value)
{
	// 1. 상체가 없으면 카메라 위치를 알 수 없으므로 중단
	if (!UpperBodyInstance) return;

	// 2. 상체의 카메라 가져오기
	UCameraComponent* TargetCamera = UpperBodyInstance->FrontCamera;
	if (!TargetCamera) return;

	// 3. 레이캐스트(LineTrace) 설정
	FVector Start = TargetCamera->GetComponentLocation();
	FVector End = Start + (TargetCamera->GetForwardVector() * 1000.0f); // 1000cm 거리

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(UpperBodyInstance);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult, Start, End, ECC_Visibility, QueryParams
	);

	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();

		// [수정됨] 인터페이스 호출 방식 변경 (Execute_Interact -> Cast 후 직접 호출)
		if (HitActor && HitActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
		{
			// 디버그: 초록색 선 (성공)
			DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 2.0f, 0, 1.0f);

			// ★ 중요: C++ 인터페이스는 Cast해서 직접 함수를 부릅니다.
			if (IInteractableInterface* Interface = Cast<IInteractableInterface>(HitActor))
			{
				Interface->Interact(this);

				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
					FString::Printf(TEXT("상호작용 성공: %s"), *HitActor->GetName()));
			}
		}
		else
		{
			// 디버그: 노란색 선 (물체는 맞았지만 상호작용 대상 아님)
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 2.0f, 0, 1.0f);
		}
	}
	else
	{
		// 디버그: 빨간색 선 (허공)
		DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 2.0f, 0, 1.0f);
	}
}

void ASoloTesterCharacter::OnAttackHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 부모의 공격 로직을 쓰므로 이 함수는 사용되지 않을 수 있지만,
	// 헤더에 선언했으므로 빈 껍데기라도 둬야 에러가 안 납니다.
}