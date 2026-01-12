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
	if (!PartnerPS || !PartnerPS->GetOwner()) return;

	ABRPlayerController* MyPC = Cast<ABRPlayerController>(GetOwner());
	ABRPlayerController* PartnerPC = Cast<ABRPlayerController>(PartnerPS->GetOwner());

	if (MyPC && PartnerPC)
	{
		APawn* MyPawn = MyPC->GetPawn();
		APawn* PartnerPawn = PartnerPC->GetPawn();

		if (MyPawn && PartnerPawn)
		{
			// 1. 모든 물리적 연결 해제 및 충돌 무시 설정
			// 상하체 구분
			APlayerCharacter* LowerChar = Cast<APlayerCharacter>(bIsLowerBody ? MyPawn : PartnerPawn);
			AUpperBodyPawn* UpperPawn = Cast<AUpperBodyPawn>(bIsLowerBody ? PartnerPawn : MyPawn);

			// 2. 컨트롤러 교체 (Unpossess -> Possess)
			MyPC->UnPossess();
			PartnerPC->UnPossess();

			MyPC->Possess(PartnerPawn);
			PartnerPC->Possess(MyPawn);

			// 3. 재부착 (Possess 이후에 수행)
			if (UpperPawn && LowerChar)
			{
				FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
				UpperPawn->AttachToComponent(LowerChar->HeadMountPoint, AttachRules);
			}

		}

		auto FinalizeSettings = [this](ABRPlayerController* PC, bool bLower)
			{
				if (!PC) return;
				PC->SetupRoleInput(bLower);

				APawn* P = PC->GetPawn();
				if (!P) return;

				// [해결: 마우스 회전] 
				// 컨트롤러의 회전값이 Pawn에 영향을 주도록 설정
				P->bUseControllerRotationYaw = true;

				if (bLower)
				{
					if (ACharacter* Char = Cast<ACharacter>(P))
					{
						// 마우스 좌우 회전이 안 된다면 이 두 값이 핵심입니다.
						Char->bUseControllerRotationYaw = true;
						Char->GetCharacterMovement()->bOrientRotationToMovement = false;
					}
				}

				PC->SetViewTarget(P);
				PC->ResetIgnoreLookInput();
				PC->ResetIgnoreMoveInput();
			};

		FinalizeSettings(MyPC, bIsLowerBody);
		FinalizeSettings(PartnerPC, PartnerPS->bIsLowerBody);

		UE_LOG(LogTemp, Log, TEXT("물리 초기화 및 스왑 설정 완료"));
	}
}