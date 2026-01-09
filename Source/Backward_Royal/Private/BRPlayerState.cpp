// BRPlayerState.cpp
#include "BRPlayerState.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UpperBodyPawn.h"
#include "PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

ABRPlayerState::ABRPlayerState()
{
	TeamNumber = 0;
	bIsHost = false;
	bIsReady = false;
	bIsLowerBody = true; // 기본값은 하체
	ConnectedPlayerIndex = -1; // 기본값은 연결 없음
}

void ABRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRPlayerState, TeamNumber);
	DOREPLIFETIME(ABRPlayerState, bIsHost);
	DOREPLIFETIME(ABRPlayerState, bIsReady);
	DOREPLIFETIME(ABRPlayerState, bIsLowerBody);
	DOREPLIFETIME(ABRPlayerState, ConnectedPlayerIndex);
}

void ABRPlayerState::BeginPlay()
{
	Super::BeginPlay();
}

void ABRPlayerState::SetTeamNumber(int32 NewTeamNumber)
{
	if (HasAuthority())
	{
		int32 OldTeam = TeamNumber;
		TeamNumber = NewTeamNumber;
		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = TEXT("Unknown Player");
		}
		UE_LOG(LogTemp, Log, TEXT("[팀 변경] %s: 팀 %d -> 팀 %d"), *PlayerName, OldTeam, NewTeamNumber);
		OnRep_TeamNumber();
	}
}

void ABRPlayerState::SetIsHost(bool bNewIsHost)
{
	if (HasAuthority())
	{
		bIsHost = bNewIsHost;
		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = TEXT("Unknown Player");
		}
		if (bNewIsHost)
		{
			UE_LOG(LogTemp, Log, TEXT("[방장] %s가 방장이 되었습니다."), *PlayerName);
		}
		OnRep_IsHost();
	}
}

void ABRPlayerState::ToggleReady()
{
	if (HasAuthority())
	{
		bool bWasReady = bIsReady;
		bIsReady = !bIsReady;
		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = TEXT("Unknown Player");
		}
		UE_LOG(LogTemp, Log, TEXT("[준비 상태] %s: %s -> %s"), 
			*PlayerName,
			bWasReady ? TEXT("준비 완료") : TEXT("대기 중"),
			bIsReady ? TEXT("준비 완료") : TEXT("대기 중"));
		OnRep_IsReady();
		
		// 준비 상태 변경 후 게임 시작 가능 여부 확인
		if (UWorld* World = GetWorld())
		{
			if (ABRGameState* BRGameState = World->GetGameState<ABRGameState>())
			{
				BRGameState->CheckCanStartGame();
			}
		}
	}
}

void ABRPlayerState::OnRep_TeamNumber()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::OnRep_IsHost()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::OnRep_IsReady()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::SetPlayerRole(bool bLowerBody, int32 ConnectedIndex)
{
	if (HasAuthority())
	{
		bIsLowerBody = bLowerBody;
		ConnectedPlayerIndex = ConnectedIndex;
		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = TEXT("Unknown Player");
		}
		FString RoleName = bLowerBody ? TEXT("하체") : TEXT("상체");
		UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: %s 역할 할당 (연결된 플레이어 인덱스: %d)"), 
			*PlayerName, *RoleName, ConnectedIndex);
		OnRep_PlayerRole();
	}
}

void ABRPlayerState::OnRep_PlayerRole()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::SwapControlWithPartner()
{
	if (!HasAuthority()) return;

	ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	// ConnectedPlayerIndex가 유효한지 확인
	if (!GS || !GS->PlayerArray.IsValidIndex(ConnectedPlayerIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("유효하지 않은 파트너 인덱스입니다: %d"), ConnectedPlayerIndex);
		return;
	}

	ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(GS->PlayerArray[ConnectedPlayerIndex]);
	if (!PartnerPS) return;

	// 1. 각자의 컨트롤러와 현재 조종 중인 Pawn 확보
	ABRPlayerController* MyPC = Cast<ABRPlayerController>(GetOwner());
	ABRPlayerController* PartnerPC = Cast<ABRPlayerController>(PartnerPS->GetOwner());

	if (MyPC && PartnerPC)
	{
		APawn* MyCurrentPawn = MyPC->GetPawn();
		APawn* PartnerPawn = PartnerPC->GetPawn();

		if (MyCurrentPawn && PartnerPawn)
		{
			// [필수] 빙의 전 기존 관계 정리
			MyPC->UnPossess();
			PartnerPC->UnPossess();

			// 2. 교차 빙의 (컨트롤러 스왑)
			MyPC->Possess(PartnerPawn);
			PartnerPC->Possess(MyCurrentPawn);

			// 3. 상체 부착 상태 재확인 (방어 코드)
			APlayerCharacter* LowerChar = nullptr;
			AUpperBodyPawn* UpperPawn = nullptr;

			// 현재 나의 '데이터상' 역할이 하체라면, 내가 새로 잡은 PartnerPawn이 상체여야 함
			if (bIsLowerBody)
			{
				LowerChar = Cast<APlayerCharacter>(MyCurrentPawn);
				UpperPawn = Cast<AUpperBodyPawn>(PartnerPawn);
			}
			else
			{
				LowerChar = Cast<APlayerCharacter>(PartnerPawn);
				UpperPawn = Cast<AUpperBodyPawn>(MyCurrentPawn);
			}

			if (UpperPawn && LowerChar)
			{
				FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
				UpperPawn->AttachToComponent(LowerChar->HeadMountPoint, AttachRules);
				UE_LOG(LogTemp, Log, TEXT("상체(%s)를 하체(%s)에 다시 부착했습니다."), *UpperPawn->GetName(), *LowerChar->GetName());
			}
		}

		// 4. 입력(Input Mapping Context) 및 시점 설정 동기화
		// 람다 함수를 통해 각 컨트롤러에 새로운 역할 규칙 적용
		auto ApplyRoleSettings = [this](ABRPlayerController* PC, bool bIsLower)
			{
				if (!PC) return;

				// [해결: 상체 조작 빠짐] 역할에 맞는 Input Mapping Context 교체
				// PlayerController에 Mapping Context를 스왑하는 함수가 구현되어 있어야 합니다.
				PC->SetupRoleInput(bIsLower);

				APawn* P = PC->GetPawn();
				if (!P) return;

				if (bIsLower)
				{
					// [해결: 하체 시점 전환] 하체는 캐릭터 회전을 컨트롤러에 맞춤
					P->bUseControllerRotationYaw = true;
					if (ACharacter* Character = Cast<ACharacter>(P))
					{
						// 이동 기능 활성화 확인
						Character->GetCharacterMovement()->SetDefaultMovementMode();
					}
				}
				else
				{
					// 상체는 에임 방향에 따라 회전 (프로젝트 설정에 따라 다를 수 있음)
					P->bUseControllerRotationYaw = true;
				}

				// [해결: 카메라 고정] 시점을 새로 빙의한 Pawn으로 강제 고정
				PC->SetViewTarget(P);
				// 입력 무시 상태 초기화
				PC->ResetIgnoreLookInput();
				PC->ResetIgnoreMoveInput();
			};

		// 나의 새로운 설정 적용 (현재 나의 bIsLowerBody는 이미 Orb에서 바뀐 상태여야 함)
		ApplyRoleSettings(MyPC, bIsLowerBody);
		// 파트너의 새로운 설정 적용
		ApplyRoleSettings(PartnerPC, PartnerPS->bIsLowerBody);

		UE_LOG(LogTemp, Log, TEXT("모든 조작 권한 및 입력 컨텍스트 스왑 완료"));
	}
}