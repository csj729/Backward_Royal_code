// BRGameState.cpp
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"

/** 로비 표시용: 비어 있거나 UserUID와 같으면 "Player N"으로 저장. 패턴 없음. */
bool ShouldUseFallbackDisplayName(const FString& PlayerName, const FString& UserUID)
{
	return PlayerName.IsEmpty() || PlayerName == UserUID;
}

ABRGameState::ABRGameState()
{
	PlayerCount = 0;
	bCanStartGame = false;
}

void ABRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRGameState, PlayerCount);
	DOREPLIFETIME(ABRGameState, PlayerListForDisplay);
	DOREPLIFETIME(ABRGameState, LobbyEntrySlots);
	DOREPLIFETIME(ABRGameState, LobbyTeamSlots);
	DOREPLIFETIME(ABRGameState, bCanStartGame);
	DOREPLIFETIME(ABRGameState, RoomTitle);
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
		// 서버가 플레이어 목록을 채워 복제 → 클라이언트도 동일 목록으로 UI 표시. "Player N" 폴백 없음 → 이름 없으면 공란, ServerSetPlayerName 도착 시 갱신
		PlayerListForDisplay.Empty();
		for (int32 i = 0; i < PlayerArray.Num(); i++)
		{
			if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PlayerArray[i]))
			{
				FBRUserInfo Info = BRPS->GetUserInfo();
				Info.PlayerIndex = i;
				UE_LOG(LogTemp, Warning, TEXT("[로비이름] UpdatePlayerList | [%d] PlayerName='%s' UserUID='%s'"), i, *Info.PlayerName, *Info.UserUID);
				PlayerListForDisplay.Add(Info);
			}
		}

		// 로비 Entry / SelectTeam 슬롯 초기화·갱신 (서버만)
		const int32 NumEntrySlots = 8;
		const int32 NumTeamSlots = 8; // 4팀 * 2슬롯
		if (LobbyEntrySlots.Num() != NumEntrySlots)
		{
			LobbyEntrySlots.SetNum(NumEntrySlots);
			for (int32 i = 0; i < NumEntrySlots; i++) LobbyEntrySlots[i] = -1;
		}
		if (LobbyTeamSlots.Num() != NumTeamSlots)
		{
			LobbyTeamSlots.SetNum(NumTeamSlots);
			for (int32 i = 0; i < NumTeamSlots; i++) LobbyTeamSlots[i] = -1;
		}
		// 나간 플레이어 인덱스는 슬롯에서 제거
		auto IsValidPlayerIndex = [this](int32 Idx) -> bool
		{
			return Idx >= 0 && Idx < PlayerArray.Num() && PlayerArray[Idx];
		};
		for (int32 i = 0; i < LobbyEntrySlots.Num(); i++)
		{
			if (!IsValidPlayerIndex(LobbyEntrySlots[i])) LobbyEntrySlots[i] = -1;
		}
		for (int32 i = 0; i < LobbyTeamSlots.Num(); i++)
		{
			if (!IsValidPlayerIndex(LobbyTeamSlots[i])) LobbyTeamSlots[i] = -1;
		}
		// 새로 들어온 플레이어를 Entry 첫 빈 자리에 배치
		for (int32 i = 0; i < PlayerArray.Num(); i++)
		{
			bool bFound = false;
			for (int32 k : LobbyEntrySlots) { if (k == i) { bFound = true; break; } }
			if (!bFound)
			{
				for (int32 k : LobbyTeamSlots) { if (k == i) { bFound = true; break; } }
			}
			if (bFound) continue;
			for (int32 j = 0; j < LobbyEntrySlots.Num(); j++)
			{
				if (LobbyEntrySlots[j] == -1) { LobbyEntrySlots[j] = i; break; }
			}
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

	// 팀 배정 후, 팀 순서(Players = 랜덤 팀 순)에 따라 하체/상체 역할 재할당
	// 규칙: 모든 팀 공통 하체→상체 (1팀 하체→상체, 2팀 하체→상체, 3팀 하체→상체, …)
	// PlayerIndices[i] = Players[i]의 PlayerArray 인덱스 (셔플 시 같이 스왑했으므로 현재 대응)
	for (int32 i = 0; i < NumPlayers; i++)
	{
		const int32 PosInTeam = i % 2;      // 0=팀 내 첫 번째(하체), 1=팀 내 두 번째(상체)
		const bool bLower = (PosInTeam == 0);
		int32 ConnectedIdx = -1;
		if (bLower && (i + 1) < NumPlayers)
		{
			ConnectedIdx = PlayerIndices[i + 1]; // 상체 쪽의 PlayerArray 인덱스
		}
		else if (!bLower && (i - 1) >= 0)
		{
			ConnectedIdx = PlayerIndices[i - 1]; // 하체 쪽의 PlayerArray 인덱스
		}
		Players[i]->SetPlayerRole(bLower, ConnectedIdx);
	}

	// 팀 배정 후 모든 플레이어를 자동으로 준비 완료 상태로 설정
	for (ABRPlayerState* BRPS : Players)
	{
		if (BRPS && IsValid(BRPS))
		{
			// 준비 상태가 아닌 경우에만 준비 완료로 설정
			if (!BRPS->bIsReady)
			{
				BRPS->bIsReady = true;
				BRPS->OnRep_IsReady();
				FString PlayerName = BRPS->GetPlayerName();
				if (PlayerName.IsEmpty())
				{
					PlayerName = TEXT("Unknown Player");
				}
				UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] %s 자동 준비 완료"), *PlayerName);
			}
		}
	}

	// 로비 UI 갱신: Entry 비우기, SelectTeam(팀별 상체/하체)에 배정 결과 반영
	const int32 NumEntrySlots = 8;
	const int32 NumTeamSlots = 8; // 4팀 * 2슬롯 (1P=하체, 2P=상체)
	if (LobbyEntrySlots.Num() != NumEntrySlots)
	{
		LobbyEntrySlots.SetNum(NumEntrySlots);
	}
	for (int32 i = 0; i < NumEntrySlots; i++)
	{
		LobbyEntrySlots[i] = -1;
	}
	if (LobbyTeamSlots.Num() != NumTeamSlots)
	{
		LobbyTeamSlots.SetNum(NumTeamSlots);
	}
	for (int32 i = 0; i < NumTeamSlots; i++)
	{
		LobbyTeamSlots[i] = -1;
	}
	// 팀 순서대로: 팀0 하체=PlayerIndices[0], 상체=PlayerIndices[1], 팀1 하체=[2], 상체=[3], ...
	for (int32 i = 0; i < NumPlayers && i < NumTeamSlots; i++)
	{
		LobbyTeamSlots[i] = PlayerIndices[i];
	}

	// 게임 시작 가능 여부 확인
	CheckCanStartGame();

	UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 완료: 총 %d개 팀 생성, 모든 플레이어 준비 완료"), (NumPlayers + 1) / 2);
	OnPlayerListChanged.Broadcast(); // WBP_Entry 비우기 + WBP_SelectTeam_0~3 갱신
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

void ABRGameState::OnRep_PlayerListForDisplay()
{
	// 클라이언트: 복제된 목록 수신 시 UI 갱신
	OnPlayerListChanged.Broadcast();
}

void ABRGameState::OnRep_CanStartGame()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

TArray<FBRUserInfo> ABRGameState::GetAllPlayerUserInfo() const
{
	// 서버가 채운 PlayerListForDisplay가 복제되므로, 서버·클라이언트 모두 이 배열로 UI 표시
	if (PlayerListForDisplay.Num() > 0)
	{
		return PlayerListForDisplay;
	}
	// 폴백: 아직 한 번도 UpdatePlayerList가 호출되지 않은 경우(초기 등)
	TArray<FBRUserInfo> UserInfoArray;
	for (int32 i = 0; i < PlayerArray.Num(); i++)
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PlayerArray[i]))
		{
			FBRUserInfo UserInfo = BRPS->GetUserInfo();
			UserInfo.PlayerIndex = i;
			UserInfoArray.Add(UserInfo);
		}
	}
	return UserInfoArray;
}

FBRUserInfo ABRGameState::GetPlayerUserInfo(int32 PlayerIndex) const
{
	FBRUserInfo UserInfo;
	
	if (PlayerIndex >= 0 && PlayerIndex < PlayerArray.Num())
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PlayerArray[PlayerIndex]))
		{
			UserInfo = BRPS->GetUserInfo();
			UserInfo.PlayerIndex = PlayerIndex;
		}
	}
	
	return UserInfo;
}

TArray<FBRUserInfo> ABRGameState::GetLobbyEntryDisplayList() const
{
	TArray<FBRUserInfo> Out;
	Out.SetNum(8);
	for (int32 i = 0; i < 8 && i < LobbyEntrySlots.Num(); i++)
	{
		int32 Pidx = LobbyEntrySlots[i];
		if (Pidx >= 0 && Pidx < PlayerArray.Num())
		{
			// 서버가 채운 PlayerListForDisplay 우선 사용 → 클라이언트는 복제된 목록으로 올바른 이름 표시
			if (Pidx < PlayerListForDisplay.Num())
			{
				Out[i] = PlayerListForDisplay[Pidx];
				Out[i].PlayerIndex = Pidx;
			}
			else
			{
				Out[i] = GetPlayerUserInfo(Pidx);
			}
			// "Player N" 폴백 제거: 이름 없으면 공란으로 두고, ServerSetPlayerName 도착 시 갱신
		}
		// else 빈 슬롯은 기본 FBRUserInfo(PlayerIndex=-1, PlayerName 빈) → UI에서 공란 표시
	}
	return Out;
}

FBRUserInfo ABRGameState::GetLobbyTeamSlotInfo(int32 TeamIndex, int32 SlotIndex) const
{
	FBRUserInfo Empty;
	int32 Idx = TeamIndex * 2 + SlotIndex;
	if (LobbyTeamSlots.Num() <= Idx || Idx < 0) return Empty;
	int32 Pidx = LobbyTeamSlots[Idx];
	if (Pidx < 0 || Pidx >= PlayerArray.Num()) return Empty;
	// 대기열과 동일: 서버가 채운 PlayerListForDisplay 우선 사용 → 복제 후 클라이언트에서도 이름이 안정적으로 표시됨
	if (Pidx < PlayerListForDisplay.Num())
	{
		FBRUserInfo Info = PlayerListForDisplay[Pidx];
		Info.TeamID = TeamIndex + 1;
		Info.PlayerIndex = SlotIndex;  // 0=1P, 1=2P
		return Info;
	}
	FBRUserInfo Info = GetPlayerUserInfo(Pidx);
	Info.TeamID = TeamIndex + 1;
	Info.PlayerIndex = SlotIndex;
	return Info;
}

FBRUserInfo ABRGameState::GetLobbyTeamSlotInfoByTeamIDAndPlayerIndex(int32 TeamID, int32 PlayerIndex) const
{
	FBRUserInfo Empty;
	if (TeamID < 1 || TeamID > 4 || (PlayerIndex != 0 && PlayerIndex != 1)) return Empty;
	// 각 플레이어의 TeamID(TeamNumber)·PlayerIndex(1P=0, 2P=1)로 찾기 → 서버/클라이언트 모두 복제된 PlayerState 기준으로 표시
	for (int32 i = 0; i < PlayerArray.Num(); i++)
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PlayerArray[i]))
		{
			if (BRPS->TeamNumber != TeamID) continue;
			int32 Slot = BRPS->bIsLowerBody ? 0 : 1;  // 0=1P, 1=2P
			if (Slot != PlayerIndex) continue;
			FBRUserInfo Info = BRPS->GetUserInfo();
			Info.TeamID = TeamID;
			Info.PlayerIndex = PlayerIndex;
			return Info;
		}
	}
	return Empty;
}

void ABRGameState::OnRep_LobbySlots()
{
	OnPlayerListChanged.Broadcast();
}

/** 대기열(LobbyEntrySlots)을 압축: 채워진 슬롯을 앞으로 모으고, 빈 슬롯(-1)은 뒤로. 인덱스 2번이 비면 3번 이후가 2번부터 채워짐 */
void ABRGameState::CompactLobbyEntrySlots()
{
	if (LobbyEntrySlots.Num() < 8) return;
	TArray<int32> Compacted;
	Compacted.Reserve(8);
	for (int32 i = 0; i < LobbyEntrySlots.Num(); i++)
	{
		if (LobbyEntrySlots[i] >= 0)
		{
			Compacted.Add(LobbyEntrySlots[i]);
		}
	}
	const int32 NumEntrySlots = 8;
	while (Compacted.Num() < NumEntrySlots)
	{
		Compacted.Add(-1);
	}
	LobbyEntrySlots = MoveTemp(Compacted);
}

bool ABRGameState::AssignPlayerToLobbyTeam(int32 PlayerIndex, int32 TeamIndex, int32 SlotIndex)
{
	if (!HasAuthority()) return false;
	if (TeamIndex < 0 || TeamIndex > 3 || SlotIndex < 0 || SlotIndex > 1) return false;
	if (PlayerIndex < 0 || PlayerIndex >= PlayerArray.Num()) return false;
	const int32 Flat = TeamIndex * 2 + SlotIndex;
	if (LobbyTeamSlots.Num() <= Flat || LobbyEntrySlots.Num() < 8) return false;

	// 이미 이 슬롯에 있으면 아무것도 하지 않음 (같은 버튼 다시 클릭 시 대기열로 돌아가는 버그 방지)
	if (LobbyTeamSlots[Flat] == PlayerIndex)
	{
		return true;
	}

	// Entry에서 해당 플레이어 제거
	bool bFoundInEntry = false;
	for (int32 i = 0; i < LobbyEntrySlots.Num(); i++)
	{
		if (LobbyEntrySlots[i] == PlayerIndex)
		{
			LobbyEntrySlots[i] = -1;
			bFoundInEntry = true;
			break;
		}
	}
	// 기존 팀 슬롯에 있던 플레이어(다른 사람)는 Entry 첫 빈 자리로
	int32 OldPlayer = LobbyTeamSlots[Flat];
	if (OldPlayer >= 0 && OldPlayer != PlayerIndex)
	{
		for (int32 i = 0; i < LobbyEntrySlots.Num(); i++)
		{
			if (LobbyEntrySlots[i] == -1) { LobbyEntrySlots[i] = OldPlayer; break; }
		}
	}
	LobbyTeamSlots[Flat] = PlayerIndex;
	if (!bFoundInEntry)
	{
		// 이미 팀 다른 슬롯에 있었을 수 있음 → 그 슬롯 비우기
		for (int32 i = 0; i < LobbyTeamSlots.Num(); i++)
		{
			if (LobbyTeamSlots[i] == PlayerIndex) LobbyTeamSlots[i] = -1;
		}
	}

	// 팀 선택한 플레이어의 UserInfo(PlayerState) 갱신 → 서버 GameState PlayerListForDisplay 반영
	if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PlayerArray[PlayerIndex]))
	{
		const int32 NewTeamNumber = TeamIndex + 1;  // TeamIndex 0=1팀, 1=2팀, ...
		const bool bLowerBody = (SlotIndex == 0);   // 0=1P(하체), 1=2P(상체)
		const int32 OtherFlat = TeamIndex * 2 + (1 - SlotIndex);
		const int32 PartnerIndex = (LobbyTeamSlots.IsValidIndex(OtherFlat)) ? LobbyTeamSlots[OtherFlat] : -1;

		BRPS->SetTeamNumber(NewTeamNumber);
		BRPS->SetPlayerRole(bLowerBody, (PartnerIndex >= 0) ? PartnerIndex : -1);
		// 같은 팀 파트너가 있으면 파트너의 ConnectedPlayerIndex도 갱신
		if (PartnerIndex >= 0 && PartnerIndex < PlayerArray.Num())
		{
			if (ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(PlayerArray[PartnerIndex]))
			{
				PartnerPS->SetPlayerRole(PartnerPS->bIsLowerBody, PlayerIndex);
			}
		}
		// 팀 버튼으로 배치된 플레이어 자동 준비 완료 (랜덤 버튼과 동일)
		if (!BRPS->bIsReady)
		{
			BRPS->bIsReady = true;
			BRPS->OnRep_IsReady();
		}
	}

	CheckCanStartGame();

	// 대기열 압축: 빈 자리 제거 후 뒤 플레이어를 앞으로 채움 (예: 2번 자리 비면 3번부터 2번 자리로 당겨짐)
	CompactLobbyEntrySlots();
	OnPlayerListChanged.Broadcast();
	return true;
}

bool ABRGameState::MovePlayerToLobbyEntry(int32 TeamIndex, int32 SlotIndex)
{
	if (!HasAuthority()) return false;
	if (TeamIndex < 0 || TeamIndex > 3 || SlotIndex < 0 || SlotIndex > 1) return false;
	const int32 Flat = TeamIndex * 2 + SlotIndex;
	if (LobbyTeamSlots.Num() <= Flat || LobbyEntrySlots.Num() < 8) return false;
	int32 PlayerIndex = LobbyTeamSlots[Flat];
	if (PlayerIndex < 0) return false;
	LobbyTeamSlots[Flat] = -1;

	// 대기열로 나간 플레이어의 UserInfo(PlayerState) 초기화 → 서버 GameState 반영
	if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PlayerArray[PlayerIndex]))
	{
		BRPS->SetTeamNumber(0);
		BRPS->SetPlayerRole(true, -1);
		// 같은 팀에 남아 있는 파트너가 있으면 그쪽 연결 해제
		const int32 OtherFlat = TeamIndex * 2 + (1 - SlotIndex);
		if (LobbyTeamSlots.IsValidIndex(OtherFlat))
		{
			const int32 PartnerIndex = LobbyTeamSlots[OtherFlat];
			if (PartnerIndex >= 0 && PartnerIndex < PlayerArray.Num())
			{
				if (ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(PlayerArray[PartnerIndex]))
				{
					PartnerPS->SetPlayerRole(PartnerPS->bIsLowerBody, -1);
				}
			}
		}
	}

	// 같은 플레이어가 이미 대기열에 있으면 제거 (중복 표시 방지: 대기열 버튼 이중 호출 등)
	for (int32 i = 0; i < LobbyEntrySlots.Num(); i++)
	{
		if (LobbyEntrySlots[i] == PlayerIndex)
		{
			LobbyEntrySlots[i] = -1;
			break;
		}
	}

	for (int32 i = 0; i < LobbyEntrySlots.Num(); i++)
	{
		if (LobbyEntrySlots[i] == -1)
		{
			LobbyEntrySlots[i] = PlayerIndex;
			// 대기열에 넣은 뒤 압축해서 순서 유지 (뒤에 빈 칸이 있으면 당겨서 채움)
			CompactLobbyEntrySlots();
			OnPlayerListChanged.Broadcast();
			return true;
		}
	}
	LobbyTeamSlots[Flat] = PlayerIndex; // 빈 자리 없으면 원복
	return false;
}

FString ABRGameState::GetHostPlayerName() const
{
	for (int32 i = 0; i < PlayerArray.Num(); i++)
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PlayerArray[i]))
		{
			if (BRPS->bIsHost)
			{
				FString Name = BRPS->GetPlayerName();
				return Name.IsEmpty() ? FString(TEXT("Host")) : Name;
			}
		}
	}
	return FString();
}

void ABRGameState::SetRoomTitle(const FString& InRoomTitle)
{
	if (HasAuthority())
	{
		RoomTitle = InRoomTitle;
		UE_LOG(LogTemp, Log, TEXT("[방 제목] 서버 설정: %s"), *RoomTitle);
		OnRep_RoomTitle();
	}
}

void ABRGameState::OnRep_RoomTitle()
{
	// UI 갱신 시 활용 가능
}

FString ABRGameState::GetRoomTitleDisplay() const
{
	if (!RoomTitle.IsEmpty())
	{
		return RoomTitle;
	}
	FString HostName = GetHostPlayerName();
	if (HostName.IsEmpty())
	{
		return FString(TEXT("Host's Game"));
	}
	return HostName + TEXT("'s Game");
}
