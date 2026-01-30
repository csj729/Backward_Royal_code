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
#include "BRAttackComponent.h" // 공격 컴포넌트

ASoloTesterCharacter::ASoloTesterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 시점 설정 (마우스 회전)
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

	// ★ 중요: 내 몸(Mesh)이 보여야 공격 모션도 보임
	if (GetMesh()) GetMesh()->SetOwnerNoSee(false);

	// 상체(카메라) 스폰
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

	// 시선 동기화
	if (UpperBodyInstance && GetController())
	{
		UpperBodyInstance->SetActorRotation(GetControlRotation());
	}
	UpperBodyAimRotation = GetControlRotation();
}

// [★핵심 변경] 메인 캐릭터와 100% 동일하게 작동
void ASoloTesterCharacter::RelayAttack(const FInputActionValue& Value)
{
	// 부모 클래스(BaseCharacter)에 있는 공격 요청 함수를 호출합니다.
	// 이 함수가 자동으로 다음을 처리합니다:
	// 1. 무기 들었는지 확인 -> 무기 공격
	// 2. 맨손인지 확인 -> 왼손/오른손 번갈아 공격
	// 3. 애니메이션 노티파이를 통해 데미지 판정(AttackComponent) 활성화
	RequestAttack();

	// if(GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("공격 요청 (RequestAttack)"));
}

void ASoloTesterCharacter::ForceAttack()
{
	RequestAttack();
}

void ASoloTesterCharacter::RelayInteract(const FInputActionValue& Value)
{
	if (!UpperBodyInstance) return;

	UCameraComponent* TargetCamera = UpperBodyInstance->FrontCamera;
	if (!TargetCamera) return;

	// 레이캐스트 길이 넉넉하게 (10m)
	FVector Start = TargetCamera->GetComponentLocation();
	FVector End = Start + (TargetCamera->GetForwardVector() * 1000.0f);

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

		if (HitActor && HitActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
		{
			DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 2.0f, 0, 1.0f);

			if (IInteractableInterface* Interface = Cast<IInteractableInterface>(HitActor))
			{
				// 나(this)를 전달하여 무기가 나에게 장착되도록 함
				Interface->Interact(this);

				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
					FString::Printf(TEXT("상호작용: %s"), *HitActor->GetName()));
			}
		}
		else
		{
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 2.0f, 0, 1.0f);
		}
	}
	else
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 2.0f, 0, 1.0f);
	}
}