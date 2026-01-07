// BRPlayerState.cpp
#include "BRPlayerState.h"
#include "BRGameState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

ABRPlayerState::ABRPlayerState()
{
	TeamNumber = 0;
	bIsHost = false;
	bIsReady = false;
}

void ABRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRPlayerState, TeamNumber);
	DOREPLIFETIME(ABRPlayerState, bIsHost);
	DOREPLIFETIME(ABRPlayerState, bIsReady);
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

