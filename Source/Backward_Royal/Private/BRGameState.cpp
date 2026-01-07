// BRGameState.cpp
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"

ABRGameState::ABRGameState()
{
	PlayerCount = 0;
	bCanStartGame = false;
}

void ABRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRGameState, PlayerCount);
	DOREPLIFETIME(ABRGameState, bCanStartGame);
}

void ABRGameState::BeginPlay()
{
	Super::BeginPlay();
}

void ABRGameState::UpdatePlayerList()
{
	if (HasAuthority())
	{
		int32 OldCount = PlayerCount;
		PlayerCount = PlayerArray.Num();
		if (OldCount != PlayerCount)
		{
			UE_LOG(LogTemp, Log, TEXT("[플레이어 목록] 업데이트: %d -> %d명"), OldCount, PlayerCount);
		}
		OnRep_PlayerCount();
		CheckCanStartGame();
		OnPlayerListChanged.Broadcast();
	}
}

void ABRGameState::CheckCanStartGame()
{
	if (HasAuthority())
	{
		bool bCanStart = (PlayerCount >= MinPlayers && PlayerCount <= MaxPlayers && AreAllPlayersReady());
		if (bCanStart != bCanStartGame)
		{
			bCanStartGame = bCanStart;
			if (bCanStart)
			{
				UE_LOG(LogTemp, Log, TEXT("[게임 시작] 조건 만족: 게임 시작 가능"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[게임 시작] 조건 불만족: 플레이어 수=%d/%d-%d, 모든 준비=%s"), 
					PlayerCount, MinPlayers, MaxPlayers,
					AreAllPlayersReady() ? TEXT("예") : TEXT("아니오"));
			}
			OnRep_CanStartGame();
		}
	}
}

void ABRGameState::AssignRandomTeams()
{
	if (!HasAuthority())
		return;

	UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 시작"));
	TArray<ABRPlayerState*> Players;
	TArray<int32> PlayerIndices; // PlayerArray에서의 원래 인덱스 저장
	
	for (int32 i = 0; i < PlayerArray.Num(); i++)
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PlayerArray[i]))
		{
			Players.Add(BRPS);
			PlayerIndices.Add(i); // 원래 인덱스 저장
		}
	}

	int32 NumPlayers = Players.Num();
	UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 총 %d명의 플레이어"), NumPlayers);

	// 플레이어를 섞기 (인덱스도 함께 이동)
	for (int32 i = NumPlayers - 1; i > 0; i--)
	{
		int32 j = FMath::RandRange(0, i);
		Players.Swap(i, j);
		PlayerIndices.Swap(i, j); // 인덱스도 함께 스왑
	}

	// 2인 1조로 팀 배정
	for (int32 i = 0; i < NumPlayers; i++)
	{
		int32 TeamNumber = (i / 2) + 1; // 0,1 -> 팀1, 2,3 -> 팀2, ...
		FString PlayerName = Players[i]->GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = FString::Printf(TEXT("Player %d"), i + 1);
		}
		Players[i]->SetTeamNumber(TeamNumber);
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] %s -> 팀 %d"), *PlayerName, TeamNumber);
	}

	// 팀 배정 후, 입장 순서(원래 PlayerArray 인덱스)에 따라 하체/상체 역할 재할당
	// PlayerArray의 인덱스 순서대로 정렬된 플레이어 리스트 생성
	TArray<ABRPlayerState*> SortedPlayers;
	for (int32 i = 0; i < PlayerArray.Num(); i++)
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PlayerArray[i]))
		{
			SortedPlayers.Add(BRPS);
		}
	}

	// 입장 순서대로 하체/상체 역할 할당
	for (int32 i = 0; i < SortedPlayers.Num(); i++)
	{
		if (i == 0)
		{
			// 첫 번째 플레이어 = 하체
			SortedPlayers[i]->SetPlayerRole(true, -1);
		}
		else if (i % 2 == 0)
		{
			// 홀수 번째 플레이어 (인덱스가 짝수) = 하체
			SortedPlayers[i]->SetPlayerRole(true, -1);
		}
		else
		{
			// 짝수 번째 플레이어 = 이전 플레이어의 상체
			int32 LowerBodyPlayerIndex = i - 1;
			if (LowerBodyPlayerIndex >= 0 && LowerBodyPlayerIndex < SortedPlayers.Num())
			{
				SortedPlayers[i]->SetPlayerRole(false, LowerBodyPlayerIndex);
				SortedPlayers[LowerBodyPlayerIndex]->SetPlayerRole(true, i);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 완료: 총 %d개 팀 생성"), (NumPlayers + 1) / 2);
	OnTeamChanged.Broadcast();
}

bool ABRGameState::AreAllPlayersReady() const
{
	if (PlayerCount < MinPlayers)
		return false;

	for (APlayerState* PS : PlayerArray)
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS))
		{
			if (!BRPS->bIsReady)
				return false;
		}
	}

	return true;
}

bool ABRGameState::AreAllNonHostPlayersReady() const
{
	if (PlayerCount < MinPlayers)
		return false;

	int32 NonHostPlayerCount = 0;
	int32 ReadyNonHostPlayerCount = 0;

	for (APlayerState* PS : PlayerArray)
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS))
		{
			// 호스트가 아닌 플레이어만 확인
			if (!BRPS->bIsHost)
			{
				NonHostPlayerCount++;
				if (BRPS->bIsReady)
				{
					ReadyNonHostPlayerCount++;
				}
			}
		}
	}

	// 호스트를 제외한 모든 플레이어가 준비되었는지 확인
	// 최소 플레이어 수는 호스트를 포함하므로, 호스트를 제외한 플레이어는 (MinPlayers - 1) 이상이어야 함
	return (NonHostPlayerCount >= (MinPlayers - 1)) && (ReadyNonHostPlayerCount == NonHostPlayerCount);
}

void ABRGameState::OnRep_PlayerCount()
{
	OnPlayerListChanged.Broadcast();
}

void ABRGameState::OnRep_CanStartGame()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

