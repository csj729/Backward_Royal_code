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
	if (!GS || !GS->PlayerArray.IsValidIndex(ConnectedPlayerIndex)) return;

	ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(GS->PlayerArray[ConnectedPlayerIndex]);
	if (!PartnerPS) return;

	ABRPlayerController* MyPC = Cast<ABRPlayerController>(GetOwner());
	ABRPlayerController* PartnerPC = Cast<ABRPlayerController>(PartnerPS->GetOwner());

	if (MyPC && PartnerPC)
	{
		APawn* MyCurrentPawn = MyPC->GetPawn();
		APawn* PartnerPawn = PartnerPC->GetPawn();

		if (MyCurrentPawn && PartnerPawn)
		{
			// 1. 빙의 해제 및 교차 빙의
			MyPC->UnPossess();
			PartnerPC->UnPossess();

			MyPC->Possess(PartnerPawn);
			PartnerPC->Possess(MyCurrentPawn);

			// 2. 상체 부착 상태 재설정
			APlayerCharacter* LowerChar = nullptr;
			AUpperBodyPawn* UpperPawn = nullptr;

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
			}

			// 3. [중요] 클라이언트에게 빙의가 완료되었음을 알리고 조작을 초기화함 (떨림 방지 핵심)
			MyPC->ClientRestart(PartnerPawn);
			PartnerPC->ClientRestart(MyCurrentPawn);
		}

		auto ApplyRoleSettings = [](ABRPlayerController* PC, bool bIsLower)
			{
				if (!PC) return;

				PC->SetupRoleInput(bIsLower);

				APawn* P = PC->GetPawn();
				if (!P) return;

				// [떨림 및 회전 해결] 컨트롤러의 회전 값을 Pawn에 동기화
				P->bUseControllerRotationYaw = true;

				if (bIsLower)
				{
					if (ACharacter* Character = Cast<ACharacter>(P))
					{
						// 하체 캐릭터의 이동 모드를 걷기로 강제 설정 (떨림 방지)
						Character->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
						Character->GetCharacterMovement()->bOrientRotationToMovement = false; // 컨트롤러 방향을 따르도록
					}
				}

				// 시점 및 입력 초기화
				PC->SetViewTarget(P);
				PC->ResetIgnoreLookInput();
				PC->ResetIgnoreMoveInput();
			};

		ApplyRoleSettings(MyPC, bIsLowerBody);
		ApplyRoleSettings(PartnerPC, PartnerPS->bIsLowerBody);
	}
}