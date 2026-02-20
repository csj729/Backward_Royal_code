// BRGameMode.cpp
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "BRGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UpperBodyPawn.h"
#include "PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Algo/Sort.h"
#include "TimerManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/Paths.h"
#include "OnlineSubsystemTypes.h"

ABRGameMode::ABRGameMode()
{
	// GameState 클래스 설정
	GameStateClass = ABRGameState::StaticClass();
	PlayerStateClass = ABRPlayerState::StaticClass();
	PlayerControllerClass = ABRPlayerController::StaticClass();
	GameSessionClass = ABRGameSession::StaticClass();

	// 리슨 서버 설정
	bUseSeamlessTravel = true;

	// Stage 맵 폴백 기본값은 BRGameMode.h의 StageMapPathsFallback에서 관리 (GameMapPath, StageFolderPath와 동일)
}

void ABRGameMode::ClearGameSessionForPIEExit()
{
	GameSession = nullptr;
}

TArray<FString> ABRGameMode::GetAvailableStageMapPaths() const
{
	// 개별 맵 목록이 있으면 그대로 사용 → 이 중에서 랜덤 선택
	if (StageMapPathsFallback.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Stage 맵] 맵 목록 사용: %d개 (랜덤 선택 대상)"), StageMapPathsFallback.Num());
		return StageMapPathsFallback;
	}

	// 목록이 비어 있을 때만 Stage 폴더 스캔
	TArray<FString> Result;
	if (StageFolderPath.IsEmpty())
	{
		return Result;
	}

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage 맵] Asset Registry를 사용할 수 없습니다."));
		return Result;
	}

	TArray<FAssetData> AssetDataList;
	const FName PackagePath(*StageFolderPath);
	AssetRegistry->GetAssetsByPath(PackagePath, AssetDataList, true, false);

	const FTopLevelAssetPath WorldClassPath(TEXT("/Script/Engine"), TEXT("World"));
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetClassPath == WorldClassPath)
		{
			FString PackageName = AssetData.PackageName.ToString();
			if (!PackageName.IsEmpty())
			{
				Result.Add(PackageName);
			}
		}
	}

	if (Result.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Stage 맵] 폴더 스캔: %d개 맵 수집 (폴더: %s)"), Result.Num(), *StageFolderPath);
		return Result;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Stage 맵] 폴더에서 맵을 찾지 못했습니다. (폴더: %s)"), *StageFolderPath);
	return Result;
}

void ABRGameMode::BeginPlay()
{
	Super::BeginPlay();

	// GameState에 최소/최대 플레이어 수 설정
	if (ABRGameState* BRGameState = GetGameState<ABRGameState>())
	{
		BRGameState->MinPlayers = MinPlayers;
		BRGameState->MaxPlayers = MaxPlayers;
	}

	// 로비에서 랜덤 팀 배정 후 예약된 경우: 게임 맵 로드 후 플레이어 스폰이 끝날 때까지 지연 후 적용
	// 플래그는 ApplyRoleChangesForRandomTeams에서만 클리어 (여기서 지우면 1.5초 후 타이머에서 진입 시 플래그가 false라 적용이 스킵됨)
	if (UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance()))
	{
		if (GI->GetPendingApplyRandomTeamRoles())
		{
			ScheduleInitialRoleApplyIfNeeded();
		}
		else
		{
			// 테스트 맵 직접 실행(로비 없음): 2초 후 저장된 역할 없고 전원 하체면 자동 랜덤 팀 배정 후 상체/하체 적용
			GetWorld()->GetTimerManager().SetTimer(DirectStartRoleApplyTimerHandle, this, &ABRGameMode::TryApplyDirectStartRolesFallback, 2.0f, false);
		}
	}
}

void ABRGameMode::TryApplyDirectStartRolesFallback()
{
	UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance());
	ABRGameState* BRGameState = GetGameState<ABRGameState>();
	if (!GI || !BRGameState || GI->GetPendingApplyRandomTeamRoles())
		return;
	if (BRGameState->PlayerArray.Num() < 2)
		return;

	int32 UpperBodyCount = 0;
	for (APlayerState* PS : BRGameState->PlayerArray)
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS))
		{
			if (!BRPS->bIsLowerBody) UpperBodyCount++;
		}
	}
	if (UpperBodyCount > 0)
		return;

	// 저장된 역할 없고 전원 하체 → 랜덤 팀 배정 후 저장·적용
	BRGameState->AssignRandomTeams();
	GI->SavePendingRolesForTravel(BRGameState);
	GI->SetPendingApplyRandomTeamRoles(true);
	UE_LOG(LogTemp, Warning, TEXT("[게임 맵 직접 실행] 팀/역할 미선택 → 자동 랜덤 팀 배정 후 상체/하체 적용"));
	ApplyRoleChangesForRandomTeams();
}

void ABRGameMode::ScheduleInitialRoleApplyIfNeeded()
{
	if (bHasScheduledInitialRoleApply || !GetWorld()) return;
	bHasScheduledInitialRoleApply = true;
	// Stage02_Bushes 등 로딩이 느린 맵에서 하체 스폰이 늦는 현상 완화
	const float InitialDelay = 2.0f;
	GetWorld()->GetTimerManager().SetTimer(InitialRoleApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams, InitialDelay, false);
	UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] %.1f초 후 상체/하체 Pawn 적용 예정 (한 번만 예약)"), InitialDelay);
}

void ABRGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty()) return;

	// 게임 진행 중 입장 차단이 꺼져 있으면 통과
	if (!bBlockJoinWhenGameStarted) return;

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Standalone)
	{
		return; // 단일 플레이어/로컬에서는 차단하지 않음
	}

	// 현재 맵이 로비 맵이면 항상 입장 허용
	FString CurrentMapName = UGameplayStatics::GetCurrentLevelName(World, true);
	if (CurrentMapName.IsEmpty())
	{
		CurrentMapName = World->GetMapName();
		CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);
	}

	FString LobbyMapBase = LobbyMapPath.IsEmpty()
		? TEXT("Main_Scene")
		: FPaths::GetBaseFilename(LobbyMapPath);

	if (CurrentMapName.Equals(LobbyMapBase, ESearchCase::IgnoreCase))
	{
		return; // 로비 맵이면 입장 허용
	}

	// 로비가 아닌 맵(Stage 등)인 경우: WBP_Start로 게임맵에 들어간 경우에만 입장 차단. Stage 맵을 바로 연 경우에는 입장 허용.
	UBRGameInstance* BRGI = Cast<UBRGameInstance>(World->GetGameInstance());
	if (BRGI && BRGI->GetExcludeOwnSessionFromSearch())
	{
		ErrorMessage = TEXT("게임이 이미 진행 중입니다. 이 방에는 입장할 수 없습니다.");
		UE_LOG(LogTemp, Warning, TEXT("[PreLogin] 게임 진행 중 입장 차단 (현재 맵: %s, 로비 맵: %s)"), *CurrentMapName, *LobbyMapBase);
	}
}

void ABRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer) return;

	ABRPlayerState* BRPS = NewPlayer->GetPlayerState<ABRPlayerState>();
	ABRGameState* BRGameState = GetGameState<ABRGameState>();

	if (BRPS && BRGameState)
	{
		int32 CurrentPlayerIndex = BRGameState->PlayerArray.Num() - 1;
		UWorld* WorldForCheck = GetWorld();
		const bool bIsLocalPlayer = WorldForCheck && (WorldForCheck->GetNetMode() == NM_Standalone || NewPlayer->IsLocalController());
		UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance());

		// [UserInfo 보존] 게임 시작 Travel 직후 PostLogin인 경우 저장된 UserInfo 즉시 복원 (나머지는 RestorePendingRolesFromTravel에서)
		bool bRestoredFromTravel = GI && GI->HasPendingUserInfoForIndex(CurrentPlayerIndex);
		if (bRestoredFromTravel)
		{
			GI->RestoreUserInfoToPlayerStateForPostLogin(BRPS, CurrentPlayerIndex);
		}

		// [보존] 플레이어 이름 설정 및 로그 (Travel 복원이 아닐 때만)
		FString PlayerName = BRPS->GetPlayerName();
		UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin 진입 | bIsLocalPlayer=%d | GetPlayerName()='%s' | bRestoredFromTravel=%d"),
			bIsLocalPlayer ? 1 : 0, *PlayerName, bRestoredFromTravel ? 1 : 0);

		if (!bRestoredFromTravel)
		{
			if (PlayerName.IsEmpty())
			{
				PlayerName = FString::Printf(TEXT("Player %d"), BRGameState->PlayerArray.Num());
				UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | PlayerName 비어있어 기본값 사용: '%s'"), *PlayerName);
			}

			if (bIsLocalPlayer && GI)
			{
				FString SavedPlayerName = GI->GetPlayerName();
				if (!SavedPlayerName.IsEmpty())
				{
					PlayerName = SavedPlayerName;
					BRPS->SetPlayerName(PlayerName);
					UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | 로컬: BRPS->SetPlayerName('%s') 적용"), *PlayerName);
				}
			}
			if (!bIsLocalPlayer)
			{
				UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | 원격 클라이언트: GetPlayerName()='%s' (ServerSetPlayerName 도착 시 갱신)"), *BRPS->GetPlayerName());
			}

			// UserUID 설정: GI에 저장된 값 우선 사용 (방 입장/게임 시작 시 초기화 방지)
			if (GI)
			{
				FString UserUID;
				if (bIsLocalPlayer && !GI->GetUserUID().IsEmpty())
				{
					UserUID = GI->GetUserUID();
					UE_LOG(LogTemp, Log, TEXT("[UserInfo 보존] PostLogin | 로컬 GI UserUID 재사용: %s"), *UserUID);
				}
				else
				{
					UserUID = FString::Printf(TEXT("Player_%d_%s"), CurrentPlayerIndex, *FDateTime::Now().ToString());
					if (bIsLocalPlayer) GI->SetUserUID(UserUID);
				}
				BRPS->SetUserUID(UserUID);
				UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | SetUserUID('%s') | 최종 PlayerName='%s'"), *UserUID, *BRPS->GetPlayerName());
			}
		}

		// 클라이언트 연결 확인 — 방 생성(ListenServer/Dedicated) 시에만 상세 로그, Standalone은 최소 로그
		UWorld* World = GetWorld();
		ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
		FString NetModeString = NetMode == NM_ListenServer ? TEXT("ListenServer") : 
		                       NetMode == NM_DedicatedServer ? TEXT("DedicatedServer") : 
		                       NetMode == NM_Client ? TEXT("Client") : TEXT("Standalone");
		
		if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
		{
			UE_LOG(LogTemp, Warning, TEXT("========================================"));
			UE_LOG(LogTemp, Warning, TEXT("[서버] 클라이언트 입장 확인!"));
			UE_LOG(LogTemp, Warning, TEXT("========================================"));
			UE_LOG(LogTemp, Warning, TEXT("[서버] 플레이어 이름: %s"), *PlayerName);
			UE_LOG(LogTemp, Warning, TEXT("[서버] 현재 인원: %d/%d"), BRGameState->PlayerArray.Num(), BRGameState->MaxPlayers);
			UE_LOG(LogTemp, Warning, TEXT("[서버] 네트워크 모드: %s"), *NetModeString);
			UE_LOG(LogTemp, Warning, TEXT("[서버] ✅ 리슨 서버 모드로 정상 실행 중입니다!"));
			UE_LOG(LogTemp, Warning, TEXT("[서버] 클라이언트 연결을 받을 수 있는 상태입니다."));
			if (GEngine)
			{
				FString JoinMsg = FString::Printf(TEXT("[서버] %s 입장! (인원: %d/%d)"), 
					*PlayerName, BRGameState->PlayerArray.Num(), BRGameState->MaxPlayers);
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, JoinMsg);
			}
		}
		else
		{
			// Standalone: 방 생성 전이므로 상세 로그 생략
			UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] %s (Standalone — 방 생성 버튼으로 세션 생성)"), *PlayerName);
			UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] 참고: 실제 방(세션)을 만들려면 'CreateRoom [방이름]' 명령어 또는 방 생성 버튼을 사용하세요."));
		}

		// [보존] 방장 설정 — ListenServer/DedicatedServer에서만 적용 (Travel 복원 시 이미 복원됨)
		if (!bRestoredFromTravel && BRGameState->PlayerArray.Num() == 1 && (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer))
		{
			UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] 첫 번째 플레이어이므로 방장으로 설정됩니다."));
			BRPS->SetIsHost(true);
			// 방 제목을 서버에서 설정·복제하여 입장한 클라이언트도 "○○'s Game"으로 동일하게 표시
			FString RoomTitleStr = PlayerName.IsEmpty() ? FString(TEXT("Host's Game")) : (PlayerName + TEXT("'s Game"));
			BRGameState->SetRoomTitle(RoomTitleStr);
		}
		else if (BRGameState->PlayerArray.Num() > 1 && (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer))
		{
			// 입장한 클라이언트에게 RPC로 방 제목 즉시 전달 (복제 대기 없이 "○○'s Game" 표시)
			FString RoomTitleStr = BRGameState->RoomTitle.IsEmpty()
				? (BRGameState->GetHostPlayerName() + TEXT("'s Game"))
				: BRGameState->RoomTitle;
			if (RoomTitleStr.IsEmpty() || RoomTitleStr == TEXT("'s Game"))
			{
				RoomTitleStr = TEXT("Host's Game");
			}
			if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(NewPlayer))
			{
				BRPC->ClientReceiveRoomTitle(RoomTitleStr);
			}
		}
		else if (BRGameState->PlayerArray.Num() == 1 && NetMode == NM_Standalone)
		{
			UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] Standalone 모드 — 방 생성 버튼을 누르면 방장이 됩니다."));
		}

		// [A안] 새 접속자는 접속 순(0,1/2,3) 역할 할당 없이 대기열(관전)만. 팀/역할은 랜덤 버튼 또는 1P·2P 선택으로만 설정.
		// Travel 복원 시에는 RestorePendingRolesFromTravel / RestoreUserInfoToPlayerStateForPostLogin에서 처리.
		if (!bRestoredFromTravel)
		{
			BRPS->SetTeamNumber(0);
			BRPS->SetSpectator(true); // 대기열 = 관전. UpdatePlayerList()에서 Entry 빈 자리에 배치됨.
			if (APawn* NewPawn = NewPlayer->GetPawn())
			{
				NewPawn->SetOwner(NewPlayer);
			}
			UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: 대기열(관전) 배치 — 팀/역할은 랜덤 버튼 또는 팀 슬롯 선택으로 설정"), *PlayerName);
		}
	}

	// [보존] 플레이어 목록 업데이트
	if (BRGameState)
	{
		BRGameState->UpdatePlayerList();
		// 방 찾기 리스트에 표시되는 현재 인원 갱신 (호스트 세션만)
		if (ABRGameSession* BRSession = Cast<ABRGameSession>(GameSession))
		{
			BRSession->UpdateSessionPlayerCount(BRGameState->PlayerCount);
		}
	}

	// 늦게 들어온 클라이언트(2·3·4번째 등) 로비 UI 동기화: 입장한 그 사람에게만 RPC가 가도록 람다로 캡처
	UWorld* WorldForNet = GetWorld();
	ENetMode CurrentNetMode = WorldForNet ? WorldForNet->GetNetMode() : NM_Standalone;
	if (BRGameState->PlayerArray.Num() > 1 && (CurrentNetMode == NM_ListenServer || CurrentNetMode == NM_DedicatedServer))
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(NewPlayer))
		{
			if (!NewPlayer->IsLocalController())
			{
				UWorld* W = GetWorld();
				if (W)
				{
					TWeakObjectPtr<ABRPlayerController> WeakPC(BRPC);
					FTimerDelegate Del = FTimerDelegate::CreateLambda([WeakPC]()
					{
						if (ABRPlayerController* PC = WeakPC.Get())
						{
							PC->ClientRequestLobbyUIRefresh();
						}
					});
					FTimerHandle H1, H2;
					W->GetTimerManager().SetTimer(H1, Del, 0.6f, false);
					W->GetTimerManager().SetTimer(H2, Del, 1.5f, false);
					UE_LOG(LogTemp, Log, TEXT("[로비 UI] 입장 클라이언트에게 0.6초·1.5초 후 로비 갱신 RPC 예약 (대상 1명)"));
				}
			}
		}
	}
}

void ABRGameMode::Logout(AController* Exiting)
{
	// 방장이 나갔을 경우 새로운 방장 지정 및 역할 재할당
	if (ABRPlayerState* ExitingPS = Exiting->GetPlayerState<ABRPlayerState>())
	{
		FString ExitingPlayerName = ExitingPS->GetPlayerName();
		if (ExitingPlayerName.IsEmpty())
		{
			ExitingPlayerName = TEXT("Unknown Player");
		}
		
		if (ABRGameState* BRGameState = GetGameState<ABRGameState>())
		{
			UE_LOG(LogTemp, Log, TEXT("[플레이어 퇴장] %s가 방을 나갔습니다. (남은 인원: %d/%d)"), 
				*ExitingPlayerName, BRGameState->PlayerArray.Num() - 1, BRGameState->MaxPlayers);
			
			// 나가는 플레이어가 상체인 경우, 연결된 하체 플레이어의 연결 해제
			if (!ExitingPS->bIsLowerBody && ExitingPS->ConnectedPlayerIndex >= 0)
			{
				if (ExitingPS->ConnectedPlayerIndex < BRGameState->PlayerArray.Num())
				{
					if (ABRPlayerState* LowerBodyPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[ExitingPS->ConnectedPlayerIndex]))
					{
						LowerBodyPS->SetPlayerRole(true, -1); // 연결된 상체 없음
						UE_LOG(LogTemp, Log, TEXT("[플레이어 퇴장] 하체 플레이어의 상체 연결 해제"));
					}
				}
			}
			// 나가는 플레이어가 하체인 경우, 연결된 상체 플레이어도 처리
			else if (ExitingPS->bIsLowerBody && ExitingPS->ConnectedPlayerIndex >= 0)
			{
				if (ExitingPS->ConnectedPlayerIndex < BRGameState->PlayerArray.Num())
				{
					if (ABRPlayerState* UpperBodyPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[ExitingPS->ConnectedPlayerIndex]))
					{
						// 상체 플레이어도 나가야 하거나, 다른 하체에 연결시킬 수 있음
						// 여기서는 단순히 연결 해제만 함
						UpperBodyPS->SetPlayerRole(false, -1);
						UE_LOG(LogTemp, Log, TEXT("[플레이어 퇴장] 상체 플레이어의 하체 연결 해제"));
					}
				}
			}
			
			if (ExitingPS->bIsHost && BRGameState->PlayerArray.Num() > 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("[플레이어 퇴장] 방장이 나갔으므로 새로운 방장을 지정합니다."));
				// 다음 플레이어를 방장으로 설정
				for (APlayerState* PS : BRGameState->PlayerArray)
				{
					if (PS != ExitingPS)
					{
						if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS))
						{
							BRPS->SetIsHost(true);
							break;
						}
					}
				}
			}
		}
	}

	// Super::Logout 전에 PlayerArray에서 퇴장한 PlayerState를 먼저 제거.
	// 그래야 즉시 UpdatePlayerList()를 호출해도 나간 이름이 목록에서 빠짐.
	// (타이머로 미루면 클라이언트 입장 시 등 다른 상황에서 크래시 가능)
	ABRGameState* BRGameState = GetGameState<ABRGameState>();
	if (BRGameState)
	{
		if (APlayerState* ExitingPS = Exiting->GetPlayerState<APlayerState>())
		{
			BRGameState->PlayerArray.Remove(ExitingPS);
		}
	}

	Super::Logout(Exiting);

	// 플레이어 목록 업데이트 및 역할 재할당 (즉시 실행, 타이머 없음)
	if (BRGameState)
	{
		BRGameState->UpdatePlayerList();
		// 방 찾기 리스트에 표시되는 현재 인원 갱신 (호스트 세션만)
		if (ABRGameSession* BRSession = Cast<ABRGameSession>(GameSession))
		{
			BRSession->UpdateSessionPlayerCount(BRGameState->PlayerCount);
		}

		for (int32 i = 0; i < BRGameState->PlayerArray.Num(); i++)
		{
			if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[i]))
			{
				if (i == 0)
				{
					BRPS->SetPlayerRole(true, -1);
				}
				else if (i % 2 == 0)
				{
					BRPS->SetPlayerRole(true, -1);
				}
				else
				{
					int32 LowerBodyPlayerIndex = i - 1;
					if (LowerBodyPlayerIndex >= 0 && LowerBodyPlayerIndex < BRGameState->PlayerArray.Num())
					{
						if (ABRPlayerState* LowerBodyPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[LowerBodyPlayerIndex]))
						{
							BRPS->SetPlayerRole(false, LowerBodyPlayerIndex);
							LowerBodyPS->SetPlayerRole(true, i);
						}
					}
				}
			}
		}
	}
}

void ABRGameMode::ApplyRoleChangesForRandomTeams()
{
	if (!HasAuthority()) return;
	// 순차 스폰 진행 중엔 재진입 금지 (OnPossess 등 다른 타이머가 Staged 상태를 덮어쓰지 않도록)
	if (StagedNumTeams > 0)
		return;
	if (!UpperBodyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[랜덤 팀 적용] UpperBodyClass가 설정되지 않았습니다. BP_MainGameMode(또는 사용 중인 GameMode 블루프린트)에서 Upper Body Class에 BP_UpperBodyPawn을 할당하세요."));
		return;
	}

	UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance());
	if (!GI || !GI->GetPendingApplyRandomTeamRoles())
		return;

	ABRGameState* BRGameState = GetGameState<ABRGameState>();
	if (!BRGameState)
		return;
	// Seamless Travel 직후 2초 타이머가 클라이언트 재접속 전에 돌면 PlayerArray가 1명(호스트만) → 상체 적용 스킵 → 하체만 스폰. 2명 될 때까지 0.5초 간격 재시도
	if (BRGameState->PlayerArray.Num() < 2)
	{
		if (MinPlayerWaitRetries >= MaxMinPlayerWaitRetries)
		{
			UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 플레이어 2명 대기 %d회 초과 → 상체/하체 적용 포기 (현재 %d명)"), MaxMinPlayerWaitRetries, BRGameState->PlayerArray.Num());
			MinPlayerWaitRetries = 0;
			return;
		}
		MinPlayerWaitRetries++;
		FTimerHandle H;
		GetWorld()->GetTimerManager().SetTimer(H, this, &ABRGameMode::ApplyRoleChangesForRandomTeams, 0.5f, false);
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 플레이어 2명 대기 중 (현재 %d명), 0.5초 후 재시도 (%d/%d)"), BRGameState->PlayerArray.Num(), MinPlayerWaitRetries, MaxMinPlayerWaitRetries);
		return;
	}
	MinPlayerWaitRetries = 0;

	// 저장된 인원 수만큼 플레이어가 다 들어올 때까지 짧게 대기 (전원 하체로 나오는 현상 방지). 최대 재시도 후에는 현재 인원으로 진행
	const int32 ExpectedCount = GI->GetPendingRoleRestoreCount();
	const int32 CurrentNum = BRGameState->PlayerArray.Num();
	if (ExpectedCount > 0 && CurrentNum < ExpectedCount)
	{
		if (InitialPlayerWaitRetries >= MaxInitialPlayerWaitRetries)
		{
			UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 플레이어 대기 %d회 초과 → 현재 인원(%d명)으로 상체/하체 적용 진행 (하체만 스폰 방지)"), MaxInitialPlayerWaitRetries, CurrentNum);
			InitialPlayerWaitRetries = 0;
		}
		else
		{
			InitialPlayerWaitRetries++;
			FTimerHandle H;
			GetWorld()->GetTimerManager().SetTimer(H, this, &ABRGameMode::ApplyRoleChangesForRandomTeams, 0.5f, false);
			UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 플레이어 대기 중 (%d/%d), 0.5초 후 재시도 (%d/%d)"), CurrentNum, ExpectedCount, InitialPlayerWaitRetries, MaxInitialPlayerWaitRetries);
			return;
		}
	}
	else if (ExpectedCount == 0 && CurrentNum == 2)
	{
		// 저장 인원 수가 0(예: PIE GI 비어 있음)이면 2명에서 바로 진행 → 1팀만 적용 후 3·4번째가 들어오면 전부 하체로 남음. 2명일 때만 3초 더 대기
		if (ExpectedZeroWaitRetries < MaxExpectedZeroWaitRetries)
		{
			ExpectedZeroWaitRetries++;
			FTimerHandle H;
			GetWorld()->GetTimerManager().SetTimer(H, this, &ABRGameMode::ApplyRoleChangesForRandomTeams, 0.5f, false);
			UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 저장 인원 없음·현재 2명 → 4명 올 때까지 0.5초 후 재시도 (%d/%d)"), ExpectedZeroWaitRetries, MaxExpectedZeroWaitRetries);
			return;
		}
		ExpectedZeroWaitRetries = 0;
	}
	else
	{
		InitialPlayerWaitRetries = 0;
		ExpectedZeroWaitRetries = 0;
	}

	// 플래그는 하체 Pawn 확인 통과 후에만 클리어 (Pawn 없이 재시도할 때 플래그 유지)
	UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] ApplyRoleChangesForRandomTeams 진입 (상체/하체 Pawn 적용)"));

	// '전체 하체 Pawn 대기' 재진입 시에는 역할 복원/재계산 스킵 (이미 복원된 역할이 덮어씌워져 전원 하체가 되는 것 방지)
	const bool bIsRetryWaitingForLowerPawns = (StagedAllLowerReadyRetries > 0);
	if (!bIsRetryWaitingForLowerPawns)
	{
		// Seamless Travel 후 PlayerState가 초기화될 수 있으므로, 저장해 둔 팀/역할을 복원
		GI->RestorePendingRolesFromTravel(BRGameState);

		// 복원된 ConnectedPlayerIndex는 로비 시점 PlayerArray 기준이므로, 현재 배열 기준으로 파트너 인덱스 재계산 (Travel 후 접속 순서 변경 시 잘못된 참조 방지)
		for (int32 i = 0; i < BRGameState->PlayerArray.Num(); i++)
		{
			ABRPlayerState* BRPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[i]);
			if (!BRPS || BRPS->bIsSpectatorSlot || BRPS->TeamNumber <= 0) continue;
			int32 PartnerIndex = -1;
			for (int32 j = 0; j < BRGameState->PlayerArray.Num(); j++)
			{
				if (i == j) continue;
				ABRPlayerState* Other = Cast<ABRPlayerState>(BRGameState->PlayerArray[j]);
				if (!Other || Other->TeamNumber != BRPS->TeamNumber) continue;
				if (Other->bIsLowerBody != BRPS->bIsLowerBody) { PartnerIndex = j; break; }
			}
			BRPS->SetPlayerRole(BRPS->bIsLowerBody, PartnerIndex);
		}
		// [진단] 복원 직후 팀/플레이어 인덱스·역할
		for (int32 i = 0; i < BRGameState->PlayerArray.Num(); i++)
		{
			if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[i]))
			{
				UE_LOG(LogTemp, Log, TEXT("[진단] 복원후 PlayerArray[%d] %s | TeamNumber=%d | %s | ConnectedIdx=%d"),
					i, *BRPS->GetPlayerName(), BRPS->TeamNumber, BRPS->bIsLowerBody ? TEXT("하체") : TEXT("상체"), BRPS->ConnectedPlayerIndex);
			}
		}
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// '하체 Pawn 대기' 재진입 시: 팀 목록을 첫 진입 시 스냅샷으로 고정 (재진입 시 PlayerArray 기준으로 다시 만들면 일부가 TeamNumber 0으로 바뀌어 3명만 남아 하체3+상체1 발생)
	TArray<ABRPlayerState*> SortedByTeam;
	int32 NumPlayersInTeams = 0;
	int32 NumTeams = 0;
	if (bIsRetryWaitingForLowerPawns && PendingSortedByTeamSnapshot.Num() > 0)
	{
		SortedByTeam = PendingSortedByTeamSnapshot;
		NumPlayersInTeams = SortedByTeam.Num();
		NumTeams = NumPlayersInTeams / 2;
		UE_LOG(LogTemp, Log, TEXT("[진단] 재진입: 저장된 팀 스냅샷 사용 %d명 (하체 Pawn 대기 재시도 중 팀 수 고정)"), NumPlayersInTeams);
	}
	else
	{
		PendingSortedByTeamSnapshot.Empty();
		// 대기열·관전(TeamNumber<=0) 제외, 팀에 배정된 플레이어만 수집 후 팀 순서 + 팀 내 하체 먼저로 정렬
		for (APlayerState* PS : BRGameState->PlayerArray)
		{
			if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS))
			{
				if (BRPS->bIsSpectatorSlot || BRPS->TeamNumber <= 0) continue; // 관전·대기열 제외
				SortedByTeam.Add(BRPS);
			}
		}
		Algo::Sort(SortedByTeam, [](const ABRPlayerState* A, const ABRPlayerState* B)
		{
			if (A->TeamNumber != B->TeamNumber) return A->TeamNumber < B->TeamNumber;
			return A->bIsLowerBody && !B->bIsLowerBody; // 하체 먼저
		});
		NumPlayersInTeams = SortedByTeam.Num();
		NumTeams = NumPlayersInTeams / 2;
	}
	const int32 ExpectedLowerCount = ABRGameMode::GetExpectedLowerBodyCount(NumPlayersInTeams);
	const int32 ExpectedUpperCount = ABRGameMode::GetExpectedUpperBodyCount(NumPlayersInTeams);

	// [진단] SortedByTeam 구성 결과 (팀 배정 인원만, 관전 제외) + 고정 스폰 수
	UE_LOG(LogTemp, Log, TEXT("[진단] 팀 배정 인원(관전 제외): %d명, 팀 수 %d (고정: 하체 %d명, 상체 %d명)"), NumPlayersInTeams, NumTeams, ExpectedLowerCount, ExpectedUpperCount);
	for (int32 i = 0; i < SortedByTeam.Num(); i++)
	{
		if (const ABRPlayerState* PS = SortedByTeam[i])
		{
			UE_LOG(LogTemp, Log, TEXT("[진단]   Sorted[%d] 팀%d %s %s"), i, PS->TeamNumber, PS->bIsLowerBody ? TEXT("하체") : TEXT("상체"), *PS->GetPlayerName());
		}
	}

	// [안전장치] 팀에 배정된 사람이 없거나, 일부만 팀 배정됐거나, 역할 분포/팀별 쌍이 잘못된 경우 게임 맵에서 랜덤 팀/역할 재배정
	// (총 4명인데 팀 배정 2명이면 팀 수 1 → 상체 1명만 스폰 → 하체3+상체1 현상)
	if (!bIsRetryWaitingForLowerPawns)
	{
		int32 TotalBRPS = 0;
		for (APlayerState* PS : BRGameState->PlayerArray) { if (Cast<ABRPlayerState>(PS)) TotalBRPS++; }
		// 팀 미배정 인원이 있으면(복원 실패 등) 전원 팀 배정 → 하체/상체 수를 고정 규칙대로 맞춤
		const bool bSomeNotInTeam = (TotalBRPS >= 2 && (TotalBRPS % 2 == 0) && NumPlayersInTeams < TotalBRPS);
		int32 UpperCountInSorted = 0;
		int32 LowerCountInSorted = 0;
		for (const ABRPlayerState* PS : SortedByTeam)
		{
			if (PS && !PS->bIsLowerBody) UpperCountInSorted++;
			else if (PS) LowerCountInSorted++;
		}
		// 고정 규칙: 플레이어 수에 따라 상체/하체 수는 반드시 N/2 each
		const bool bCountMismatch = (UpperCountInSorted != ExpectedUpperCount) || (LowerCountInSorted != ExpectedLowerCount);
		bool bTeamPairInvalid = false;
		for (int32 t = 0; t < NumTeams; t++)
		{
			ABRPlayerState* Lower = SortedByTeam.IsValidIndex(2 * t) ? SortedByTeam[2 * t] : nullptr;
			ABRPlayerState* Upper = SortedByTeam.IsValidIndex(2 * t + 1) ? SortedByTeam[2 * t + 1] : nullptr;
			if (!Lower || !Upper || !Lower->bIsLowerBody || Upper->bIsLowerBody)
			{
				bTeamPairInvalid = true;
				UE_LOG(LogTemp, Warning, TEXT("[진단] 팀 %d 역할 쌍 이상: 하체슬롯=%s(%d) 상체슬롯=%s(%d) → 랜덤 재배정 필요"),
					t + 1, Lower ? *Lower->GetPlayerName() : TEXT("없음"), Lower ? (Lower->bIsLowerBody ? 1 : 0) : -1,
					Upper ? *Upper->GetPlayerName() : TEXT("없음"), Upper ? (Upper->bIsLowerBody ? 1 : 0) : -1);
				break;
			}
		}
		const bool bNeedRandomAssign = (NumTeams < 1 && TotalBRPS >= 2)
			|| bSomeNotInTeam
			|| (NumPlayersInTeams >= 2 && (bCountMismatch || bTeamPairInvalid));
		if (bNeedRandomAssign)
		{
			if (bSomeNotInTeam)
			{
				UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 팀 미배정 인원 있음(전체 %d명 중 팀 배정 %d명) → 전원 팀/역할 재배정 (하체3+상체1 방지)"), TotalBRPS, NumPlayersInTeams);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 역할/팀 배정 없음 또는 분포/팀쌍 이상(현재 하체%d 상체%d, 고정 규칙 하체%d 상체%d) → 게임 맵에서 랜덤 팀/역할 재배정"), LowerCountInSorted, UpperCountInSorted, ExpectedLowerCount, ExpectedUpperCount);
			}
			UE_LOG(LogTemp, Warning, TEXT("[진단] 하체3+상체1 원인: 팀 미배정 또는 팀별 하체1+상체1 아님 → 안전장치로 랜덤 재배정 실행"));
			BRGameState->AssignRandomTeams();
			for (int32 i = 0; i < BRGameState->PlayerArray.Num(); i++)
			{
				ABRPlayerState* BRPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[i]);
				if (!BRPS || BRPS->bIsSpectatorSlot || BRPS->TeamNumber <= 0) continue;
				int32 PartnerIndex = -1;
				for (int32 j = 0; j < BRGameState->PlayerArray.Num(); j++)
				{
					if (i == j) continue;
					ABRPlayerState* Other = Cast<ABRPlayerState>(BRGameState->PlayerArray[j]);
					if (!Other || Other->TeamNumber != BRPS->TeamNumber) continue;
					if (Other->bIsLowerBody != BRPS->bIsLowerBody) { PartnerIndex = j; break; }
				}
				BRPS->SetPlayerRole(BRPS->bIsLowerBody, PartnerIndex);
			}
			SortedByTeam.Empty();
			for (APlayerState* PS : BRGameState->PlayerArray)
			{
				if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS))
				{
					if (BRPS->bIsSpectatorSlot || BRPS->TeamNumber <= 0) continue;
					SortedByTeam.Add(BRPS);
				}
			}
			Algo::Sort(SortedByTeam, [](const ABRPlayerState* A, const ABRPlayerState* B)
			{
				if (A->TeamNumber != B->TeamNumber) return A->TeamNumber < B->TeamNumber;
				return A->bIsLowerBody && !B->bIsLowerBody;
			});
			NumPlayersInTeams = SortedByTeam.Num();
			NumTeams = NumPlayersInTeams / 2;
		}
	}
	if (NumTeams < 1)
	{
		GI->ClearPendingApplyRandomTeamRoles();
		GI->ClearPendingRoleRestoreData();
		return;
	}

	// [강화] 전체 하체 Pawn이 준비될 때까지 대기 (로딩 빠른 맵에서 상체가 하체로 남는 현상 방지). Controller도 이름 fallback으로 검색
	auto FindControllerForPS = [World](ABRPlayerState* PS) -> APlayerController*
	{
		if (!PS) return nullptr;
		APlayerController* PC = Cast<APlayerController>(PS->GetOwningController());
		if (PC) return PC;
		const FString TargetName = PS->GetPlayerName();
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* C = It->Get();
			if (!C) continue;
			if (C->GetPlayerState<APlayerState>() == PS) return C;
			if (APlayerState* OtherPS = C->GetPlayerState<APlayerState>())
			{
				if (OtherPS->GetPlayerName() == TargetName) return C;
			}
		}
		return nullptr;
	};
	constexpr int32 MaxAllLowerReadyRetries = 20;  // 최대 6초 대기 (0.3초 × 20)
	for (int32 i = 0; i < NumTeams; i++)
	{
		ABRPlayerState* LowerPS = SortedByTeam.IsValidIndex(2 * i) ? SortedByTeam[2 * i] : nullptr;
		if (!LowerPS || !LowerPS->bIsLowerBody) continue;
		APlayerController* LowerPC = FindControllerForPS(LowerPS);
		if (!LowerPC)
		{
			// 팀2 등 원격 플레이어 Controller가 아직 없으면 스킵하지 말고 대기 (그래야 이후 팀2 상체 스폰 가능)
			if (StagedAllLowerReadyRetries < MaxAllLowerReadyRetries)
			{
				StagedAllLowerReadyRetries++;
				PendingSortedByTeamSnapshot = SortedByTeam;
				World->GetTimerManager().SetTimer(StagedAllLowerReadyHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams, 0.3f, false);
				UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 전체 하체 Pawn 대기 중 — 팀 %d Controller 없음 (%d/%d), 0.3초 후 재시도"), i + 1, StagedAllLowerReadyRetries, MaxAllLowerReadyRetries);
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 전체 하체 Pawn 대기 실패(팀 %d Controller 미등장), 팀 %d 스킵 가능"), i + 1, i + 1);
			PendingSortedByTeamSnapshot.Empty();
			break;
		}
		APlayerCharacter* LowerChar = Cast<APlayerCharacter>(LowerPC->GetPawn());
		if (!LowerChar || !IsValid(LowerChar))
		{
			if (StagedAllLowerReadyRetries < MaxAllLowerReadyRetries)
			{
				StagedAllLowerReadyRetries++;
				// 재진입 시 팀 수가 바뀌지 않도록 현재 SortedByTeam 스냅샷 저장 (재진입 시 일부가 TeamNumber 0으로 바뀌면 3명만 남아 하체3+상체1 발생)
				PendingSortedByTeamSnapshot = SortedByTeam;
				World->GetTimerManager().SetTimer(StagedAllLowerReadyHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams, 0.3f, false);
				UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 전체 하체 Pawn 대기 중 (%d/%d), 0.3초 후 재시도 (팀 스냅샷 %d명 저장)"), StagedAllLowerReadyRetries, MaxAllLowerReadyRetries, PendingSortedByTeamSnapshot.Num());
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 전체 하체 Pawn 대기 실패(재시도 %d회 초과), 팀 %d 스킵 가능"), MaxAllLowerReadyRetries, i + 1);
			PendingSortedByTeamSnapshot.Empty();
			break;
		}
	}
	StagedAllLowerReadyRetries = 0;
	PendingSortedByTeamSnapshot.Empty(); // 하체 대기 통과 또는 실패 후 스냅샷 해제

	// 순차 스폰: 1팀 하체 확인 → 1팀 상체 스폰 → 2팀 하체 확인 → 2팀 상체 스폰 … (앞사람이 전부 정상 스폰된 뒤 다음으로 진행)
	// 기존 순차 스폰 타이머가 있으면 취소
	World->GetTimerManager().ClearTimer(StagedApplyTimerHandle);
	World->GetTimerManager().ClearTimer(StagedAllLowerReadyHandle);
	StagedSortedByTeam = SortedByTeam;
	StagedNumTeams = NumTeams;
	StagedCurrentTeamIndex = 0;
	StagedPawnWaitRetriesForTeam = 0;
	StagedControllerWaitRetriesForTeam = 0;
	StagedUpperBodiesSpawnedCount = 0;
	ApplyRoleChangesForRandomTeams_ApplyOneTeam();
}

void ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam()
{
	if (!HasAuthority() || !GetWorld()) return;

	UWorld* World = GetWorld();
	if (StagedCurrentTeamIndex >= StagedNumTeams)
	{
		if (UBRGameInstance* GI = GetGameInstance<UBRGameInstance>())
		{
			GI->ClearPendingApplyRandomTeamRoles();
			GI->ClearPendingRoleRestoreData();
		}
		if (StagedUpperBodiesSpawnedCount == 0)
			UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 순차 상체 스폰 완료 but 상체 0명 스폰됨 (하체만 스폰된 상태일 수 있음)"));
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 순차 상체 스폰 완료 (고정 규칙: 하체 %d명, 상체 %d명 / 실제 상체 스폰 %d명)"), StagedNumTeams, StagedNumTeams, StagedUpperBodiesSpawnedCount);
		StagedSortedByTeam.Empty();
		StagedNumTeams = 0;
		StagedCurrentTeamIndex = 0;
		StagedPawnWaitRetriesForTeam = 0;
		StagedControllerWaitRetriesForTeam = 0;
		StagedUpperBodiesSpawnedCount = 0;
		return;
	}

	const int32 TeamIndex = StagedCurrentTeamIndex;
	ABRPlayerState* LowerPS = StagedSortedByTeam.IsValidIndex(2 * TeamIndex) ? StagedSortedByTeam[2 * TeamIndex] : nullptr;
	ABRPlayerState* UpperPS = StagedSortedByTeam.IsValidIndex(2 * TeamIndex + 1) ? StagedSortedByTeam[2 * TeamIndex + 1] : nullptr;

	if (!LowerPS || !UpperPS)
	{
		StagedPawnWaitRetriesForTeam = 0;
		StagedControllerWaitRetriesForTeam = 0;
		StagedCurrentTeamIndex++;
		World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, FMath::Max(0.01f, SpawnDelayBetweenTeams), false);
		return;
	}

	if (LowerPS->bIsLowerBody == false || UpperPS->bIsLowerBody == true)
	{
		UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 팀 %d 역할 불일치 - 스킵 (하체=%s bIsLowerBody=%d, 상체=%s bIsLowerBody=%d)"),
			TeamIndex + 1, *LowerPS->GetPlayerName(), LowerPS->bIsLowerBody ? 1 : 0, *UpperPS->GetPlayerName(), UpperPS->bIsLowerBody ? 1 : 0);
		StagedPawnWaitRetriesForTeam = 0;
		StagedControllerWaitRetriesForTeam = 0;
		StagedCurrentTeamIndex++;
		World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, FMath::Max(0.01f, SpawnDelayBetweenTeams), false);
		return;
	}

	// GetOwningController()·PlayerState 포인터가 Seamless Travel 직후 불일치할 수 있음 → World에서 PC 검색 (포인터 → 이름 매칭 fallback)
	auto FindControllerForPlayerState = [World](ABRPlayerState* PS) -> APlayerController*
	{
		if (!PS) return nullptr;
		APlayerController* PC = Cast<APlayerController>(PS->GetOwningController());
		if (PC) return PC;
		const FString TargetName = PS->GetPlayerName();
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* C = It->Get();
			if (!C) continue;
			if (C->GetPlayerState<APlayerState>() == PS)
				return C;
			// Seamless Travel 직후 서버에서 PC->PlayerState가 다른 객체를 가리킬 수 있음 → 이름으로 매칭
			if (APlayerState* OtherPS = C->GetPlayerState<APlayerState>())
			{
				if (OtherPS->GetPlayerName() == TargetName)
					return C;
			}
		}
		return nullptr;
	};
	// 해당 팀의 Controller가 서버에 올 때까지 대기 (1팀 하체→상체 완료 후 2팀 진행하듯, 이 팀 준비될 때까지 기다린 뒤 스폰)
	APlayerController* LowerPC = FindControllerForPlayerState(LowerPS);
	APlayerController* UpperPC = FindControllerForPlayerState(UpperPS);
	if (!LowerPC || !UpperPC)
	{
		if (StagedControllerWaitRetriesForTeam < MaxStagedControllerWaitRetriesForTeam)
		{
			StagedControllerWaitRetriesForTeam++;
			World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, 0.5f, false);
			UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 팀 %d Controller 대기 중 (%d/%d) — 서버에 생성되면 상체 스폰 (하체=%s 상체=%s)"),
				TeamIndex + 1, StagedControllerWaitRetriesForTeam, MaxStagedControllerWaitRetriesForTeam,
				LowerPC ? TEXT("O") : TEXT("X"), UpperPC ? TEXT("O") : TEXT("X"));
			return;
		}
		// 20초 초과 시에만 스킵 (연결 끊김 등 예외)
		UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 팀 %d Controller 대기 %d회 초과, 스킵 (하체=%s 상체=%s)"),
			TeamIndex + 1, MaxStagedControllerWaitRetriesForTeam, LowerPC ? TEXT("O") : TEXT("X"), UpperPC ? TEXT("O") : TEXT("X"));
		StagedControllerWaitRetriesForTeam = 0;
		StagedPawnWaitRetriesForTeam = 0;
		StagedCurrentTeamIndex++;
		World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, FMath::Max(0.01f, SpawnDelayBetweenTeams), false);
		return;
	}
	StagedControllerWaitRetriesForTeam = 0;  // Controller 찾음 → 해당 팀 상체 스폰 진행

	APlayerCharacter* LowerChar = Cast<APlayerCharacter>(LowerPC->GetPawn());
	if (!LowerChar || !IsValid(LowerChar))
	{
		// Stage02_Bushes 등 맵에서 하체 Pawn 스폰이 늦을 수 있음 → 같은 팀 재시도 (최대 8회, 0.3초 간격)
		if (StagedPawnWaitRetriesForTeam < MaxStagedPawnWaitRetriesPerTeam)
		{
			StagedPawnWaitRetriesForTeam++;
			World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, 0.3f, false);
			UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 팀 %d 하체 Pawn 대기 중 (%d/%d), 0.3초 후 재시도"), TeamIndex + 1, StagedPawnWaitRetriesForTeam, MaxStagedPawnWaitRetriesPerTeam);
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 팀 %d 하체 Pawn 없음(재시도 %d회 초과), 스킵"), TeamIndex + 1, MaxStagedPawnWaitRetriesPerTeam);
		StagedPawnWaitRetriesForTeam = 0;
		StagedControllerWaitRetriesForTeam = 0;
		StagedCurrentTeamIndex++;
		World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, FMath::Max(0.01f, SpawnDelayBetweenTeams), false);
		return;
	}
	StagedPawnWaitRetriesForTeam = 0; // 성공 시 재시도 카운트 초기화

	if (LowerPC->GetPawn() != LowerChar)
	{
		LowerPC->UnPossess();
		LowerPC->Possess(LowerChar);
	}

	APawn* OldUpperPawn = UpperPC->GetPawn();
	UpperPC->UnPossess();
	if (OldUpperPawn)
		OldUpperPawn->Destroy();

	AUpperBodyPawn* OldUpperOnLower = nullptr;
	for (TActorIterator<AUpperBodyPawn> It(World); It; ++It)
	{
		if (It->ParentBodyCharacter == LowerChar)
		{
			OldUpperOnLower = *It;
			break;
		}
	}
	if (OldUpperOnLower)
		OldUpperOnLower->Destroy();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = UpperPC;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AUpperBodyPawn* NewUpper = World->SpawnActor<AUpperBodyPawn>(
		UpperBodyClass, LowerChar->GetActorLocation(), LowerChar->GetActorRotation(), SpawnParams);
	if (NewUpper)
	{
		NewUpper->AttachToComponent(
			LowerChar->HeadMountPoint,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		NewUpper->ParentBodyCharacter = LowerChar;
		LowerChar->SetUpperBodyPawn(NewUpper);
		UpperPC->Possess(NewUpper);
		StagedUpperBodiesSpawnedCount++;
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 팀 %d: %s 상체 스폰 후 빙의 (순차 %d/%d)"), TeamIndex + 1, *UpperPS->GetPlayerName(), TeamIndex + 1, StagedNumTeams);
	}

	// 이전 팀(하체 확인 + 상체 스폰)이 완료된 뒤에만 다음 팀 진행. SpawnDelayBetweenTeams 동안 대기 후 다음 팀 처리.
	StagedCurrentTeamIndex++;
	if (StagedCurrentTeamIndex < StagedNumTeams)
	{
		const float Delay = FMath::Max(0.5f, SpawnDelayBetweenTeams);
		World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, Delay, false);
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 팀 %d 상체 스폰 완료, %.2f초 후 팀 %d 진행"), TeamIndex + 1, Delay, StagedCurrentTeamIndex + 1);
	}
	else
		ApplyRoleChangesForRandomTeams_ApplyOneTeam(); // 마지막 팀 처리 후 정리
}

void ABRGameMode::StartGame()
{
	if (!HasAuthority())
		return;

	UE_LOG(LogTemp, Log, TEXT("[게임 시작] 게임 시작 요청 처리 중..."));
	
	if (ABRGameState* BRGameState = GetGameState<ABRGameState>())
	{
		// 호스트인지 확인
		bool bIsHost = false;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (ABRPlayerState* BRPS = PC->GetPlayerState<ABRPlayerState>())
			{
				bIsHost = BRPS->bIsHost;
			}
		}
		
		// 호스트인 경우: 호스트를 제외한 모든 플레이어가 준비되었는지 확인
		// 호스트가 아닌 경우: 모든 플레이어가 준비되었는지 확인
		bool bCanStart = false;
		if (bIsHost)
		{
			// 호스트는 자신이 준비하지 않아도 다른 모든 플레이어가 준비되었으면 시작 가능
			bCanStart = (BRGameState->PlayerCount >= BRGameState->MinPlayers && 
			            BRGameState->PlayerCount <= BRGameState->MaxPlayers && 
			            BRGameState->AreAllNonHostPlayersReady());
			
			if (bCanStart)
			{
				UE_LOG(LogTemp, Log, TEXT("[게임 시작] 호스트: 다른 모든 플레이어가 준비 완료 - 게임 시작 가능"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[게임 시작] 호스트: 조건 불만족 - 플레이어 수=%d/%d-%d, 호스트 제외 모든 준비=%s"), 
					BRGameState->PlayerCount, BRGameState->MinPlayers, BRGameState->MaxPlayers,
					BRGameState->AreAllNonHostPlayersReady() ? TEXT("예") : TEXT("아니오"));
			}
		}
		else
		{
			// 호스트가 아닌 경우: 기존 로직 사용 (모든 플레이어가 준비되어야 함)
			BRGameState->CheckCanStartGame();
			bCanStart = BRGameState->bCanStartGame;
			
			if (!bCanStart)
			{
				UE_LOG(LogTemp, Error, TEXT("[게임 시작] 실패: 게임을 시작할 수 없습니다."));
				UE_LOG(LogTemp, Warning, TEXT("[게임 시작] 조건 확인: 플레이어 수=%d/%d-%d, 모든 준비=%s"), 
					BRGameState->PlayerCount, BRGameState->MinPlayers, BRGameState->MaxPlayers,
					BRGameState->AreAllPlayersReady() ? TEXT("예") : TEXT("아니오"));
			}
		}
		
		if (!bCanStart)
		{
			return;
		}
	}

	// 맵 선택: Stage 폴더에서 수집한 맵 중 랜덤 선택 (수집 실패 시 폴백 목록 또는 GameMapPath)
	TArray<FString> AvailableMaps = GetAvailableStageMapPaths();
	FString SelectedMapPath;
	if (bUseRandomMap && AvailableMaps.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, AvailableMaps.Num() - 1);
		SelectedMapPath = AvailableMaps[RandomIndex];
		UE_LOG(LogTemp, Log, TEXT("[게임 시작] 랜덤 맵 선택: %s (인덱스: %d/%d)"),
			*SelectedMapPath, RandomIndex + 1, AvailableMaps.Num());
	}
	else
	{
		SelectedMapPath = GameMapPath;
		UE_LOG(LogTemp, Log, TEXT("[게임 시작] 기본 맵 사용: %s"), *SelectedMapPath);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[게임 시작] 성공: 맵으로 이동 중... (%s)"), *SelectedMapPath);
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[게임 시작] 실패: World를 찾을 수 없습니다."));
		return;
	}
	
	// Travel 직전에 역할 저장 (로비에서 선택한 1P/2P·팀 포함) + 게임 맵 로드 후 상체/하체 Pawn 적용 예약
	if (UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance()))
	{
		if (ABRGameState* GS = GetGameState<ABRGameState>())
		{
			// 방만들고 바로 게임 시작한 경우: 아무도 1P/2P 선택을 안 하면 전원 기본값(하체)로 저장됨 → 게임에서 전원 하체로 나옴.
			// 상체로 설정된 플레이어가 한 명도 없고 2명 이상이면, 자동으로 랜덤 팀 배정 후 저장.
			int32 UpperBodyCount = 0;
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS))
				{
					if (!BRPS->bIsLowerBody) UpperBodyCount++;
				}
			}
			if (UpperBodyCount == 0 && GS->PlayerArray.Num() >= 2)
			{
				GS->AssignRandomTeams();
				UE_LOG(LogTemp, Warning, TEXT("[게임 시작] 팀/역할 미선택 상태 → 자동 랜덤 팀 배정 후 저장"));
			}
			GI->SavePendingRolesForTravel(GS);
			GI->SetPendingApplyRandomTeamRoles(true);  // 랜덤이 아니어도 로비 역할(1P=하체, 2P=상체) 적용을 위해 플래그 설정
			UE_LOG(LogTemp, Warning, TEXT("[게임 시작] Travel 직전 역할 저장 완료 (GameMode), 게임 맵에서 상체/하체 적용 예정"));
		}
		// WBP_Start로 게임맵 이동 시에만 방 찾기에서 "시작한 방 제외" 적용 (로비에 있을 때는 클라이언트가 방 목록에 보이도록)
		GI->SetExcludeOwnSessionFromSearch(true);
	}
	
	// PIE(Play In Editor) 환경 감지
	bool bIsPIE = World->IsPlayInEditor();
	ENetMode NetMode = World->GetNetMode();
	
	// PIE에서는 Seamless Travel이 작동하지 않으므로 일반 ServerTravel 사용
	bool bShouldUseSeamlessTravel = bUseSeamlessTravel && !bIsPIE;
	
	if (bIsPIE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[게임 시작] PIE 환경 감지 - 일반 ServerTravel 사용 (Seamless Travel 비활성화)"));
	}
	
	// 맵 이동 - PIE 환경에 따라 Travel 방식 선택
	FString TravelURL = SelectedMapPath + TEXT("?listen");
	
	if (bShouldUseSeamlessTravel)
	{
		UE_LOG(LogTemp, Log, TEXT("[게임 시작] SeamlessTravel 호출: %s"), *TravelURL);
		
		// 클라이언트에게 게임 시작 알림 (맵 이동 전에 알림)
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
				{
					// 클라이언트에게 게임 시작 알림 (RPC)
					BRPC->ClientNotifyGameStarting();
				}
			}
		}
		
		// SeamlessTravel 사용 - 클라이언트가 부드럽게 따라옵니다
		World->ServerTravel(TravelURL, true); // 두 번째 파라미터는 bAbsolute (true = 절대 경로)
	}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[게임 시작] 일반 ServerTravel 호출: %s (PIE 모드: %s)"), 
				*TravelURL, bIsPIE ? TEXT("예") : TEXT("아니오"));
			
			// 클라이언트에게 게임 시작 알림 (맵 이동 전에 알림)
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
					{
						BRPC->ClientNotifyGameStarting();
					}
				}
			}
			
			// PIE에서는 ServerTravel만으로는 클라이언트가 따라오지 않는 경우가 있으므로,
			// 원격 클라이언트에게 명시적으로 ClientTravel URL을 보내서 같은 맵으로 이동시킴
			if (bIsPIE)
			{
				// PIE 서버는 ServerConnection이 없으므로 기본 주소 사용 (클라이언트는 이 주소로 접속)
				FString ServerAddr = TEXT("127.0.0.1:7777");
				// 클라이언트 이동 URL: "host:port/MapPath" (맵 경로는 /Game/... 형식)
				FString ClientTravelURL = ServerAddr + SelectedMapPath;
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					APlayerController* PC = It->Get();
					if (!PC || PC->IsLocalController()) continue;
					if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
					{
						BRPC->ClientTravelToGameMap(ClientTravelURL);
						UE_LOG(LogTemp, Log, TEXT("[게임 시작] PIE: 원격 클라이언트에게 ClientTravel 전송: %s"), *ClientTravelURL);
					}
				}
			}

			World->ServerTravel(TravelURL, true);
		}
}

void ABRGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 서버 안정성: 모든 타이머 해제로 장시간 구동 시 메모리/참조 누수 방지
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(InitialRoleApplyTimerHandle);
		World->GetTimerManager().ClearTimer(StagedApplyTimerHandle);
		World->GetTimerManager().ClearTimer(StagedAllLowerReadyHandle);
		World->GetTimerManager().ClearTimer(DirectStartRoleApplyTimerHandle);
		World->GetTimerManager().ClearTimer(SpecTimerHandle_DeathSpectator);
		World->GetTimerManager().ClearTimer(ReturnToLobbyTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
	
	// PIE 종료 시 NavigationSystem이 World를 참조하여 GC가 되지 않는 문제는
	// Unreal Engine의 알려진 버그입니다. NavigationSystem은 World가 파괴될 때
	// 자동으로 정리되어야 하지만, PIE 종료 시 타이밍 문제로 인해 참조가 남을 수 있습니다.
	// 
	// 참고: NavigationSystem을 직접 정리하는 것은 권장되지 않으며,
	// World의 정리 순서에 문제가 있을 수 있습니다.
	// 이 문제는 주로 비동기 네비게이션 메시 빌드가 진행 중일 때 발생합니다.
	
	if (World && World->IsPlayInEditor())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] PIE 종료 - NavigationSystem은 World 파괴 시 자동으로 정리됩니다."));
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 참고: PIE 종료 시 NavigationSystem GC 경고는 Unreal Engine의 알려진 버그입니다."));
	}
}

void ABRGameMode::OnPlayerDied(ABaseCharacter* VictimCharacter)
{
	if (!VictimCharacter) return;

	UE_LOG(LogTemp, Warning, TEXT("[GameMode] 플레이어 사망 확인: %s"), *VictimCharacter->GetName());

	// 캐릭터에서 Controller 가져오기 (없으면 PlayerState에서 시도 — 하체/상체 공유 폰 등)
	AController* Controller = VictimCharacter->GetController();
	ABRPlayerState* PS = nullptr;
	if (Controller)
	{
		PS = Controller->GetPlayerState<ABRPlayerState>();
	}
	if (!PS && VictimCharacter)
	{
		// 폰에 붙은 PlayerState가 있다면 그 소유 컨트롤러 사용 (네트워크/공유 폰 대비)
		PS = VictimCharacter->GetPlayerState<ABRPlayerState>();
		if (PS)
		{
			Controller = PS->GetOwningController();
			UE_LOG(LogTemp, Log, TEXT("[GameMode] 사망자 Controller를 PlayerState 소유자로 보정"));
		}
	}

	if (PS)
	{
		// PlayerState에 사망 상태가 아직 반영 안 되었다면 여기서 확실히 처리
		if (PS->CurrentStatus != EPlayerStatus::Dead)
		{
			PS->SetPlayerStatus(EPlayerStatus::Dead);
		}
		// PlayerState를 관전(PlayerIndex 0)으로 변경. bIsSpectatorSlot=true 설정 → GameState는 PS에서 읽어 반영
		PS->SetSpectator(true);
		UE_LOG(LogTemp, Log, TEXT("[GameMode] %s (Team %d) 탈락 처리 완료"),
			*PS->GetPlayerName(), PS->TeamNumber);
	}

	// -----------------------------------------------------------
	// 팀 탈락: 같은 팀 파트너도 사망 처리 후, 2초 뒤 피해자·파트너(하체+상체) 둘 다 관전 전환
	// (인덱스로 예약해 콜백에서 팀 번호 복제 이슈 없이 전원 전환 보장)
	// -----------------------------------------------------------
	ABRGameState* GS = GetGameState<ABRGameState>();
	if (!GS || !PS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 관전 전환 스킵: GameState 또는 피해자 PS 없음"));
		return;
	}

	const int32 VictimPlayerIndex = GS->PlayerArray.Find(PS);
	if (VictimPlayerIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 관전 전환 스킵: 피해자 PlayerArray 인덱스 없음"));
		return;
	}

	int32 PartnerPlayerIndex = INDEX_NONE;
	for (APlayerState* OtherPS : GS->PlayerArray)
	{
		ABRPlayerState* BRPS = Cast<ABRPlayerState>(OtherPS);
		if (BRPS && BRPS != PS && BRPS->TeamNumber == PS->TeamNumber)
		{
			PartnerPlayerIndex = GS->PlayerArray.Find(BRPS);
			if (BRPS->CurrentStatus != EPlayerStatus::Dead)
			{
				BRPS->SetPlayerStatus(EPlayerStatus::Dead);
			}
			// 파트너도 PlayerState를 관전(PlayerIndex 0)으로 변경
			BRPS->SetSpectator(true);
			break;
		}
	}

	FTimerDelegate TimerDel;
	TimerDel.BindUObject(this, &ABRGameMode::SwitchTeamToSpectatorByPlayerIndices, VictimPlayerIndex, PartnerPlayerIndex);
	GetWorld()->GetTimerManager().SetTimer(SpecTimerHandle_DeathSpectator, TimerDel, 2.0f, false);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] 팀 %d 탈락 — 2초 후 하체·상체 관전 전환 예약 (VictimIdx=%d, PartnerIdx=%d)"),
		PS->TeamNumber, VictimPlayerIndex, PartnerPlayerIndex);

	// 승리 조건 즉시 체크 (타이머에만 의존하지 않음 — 피해자·파트너는 이미 Dead 처리됨)
	CheckAndEndGameIfWinner();
}

void ABRGameMode::SwitchTeamToSpectator(TWeakObjectPtr<ABRPlayerController> VictimPC, TWeakObjectPtr<ABRPlayerController> PartnerPC)
{
	if (VictimPC.IsValid())
	{
		VictimPC->StartSpectatingMode();
	}

	if (PartnerPC.IsValid())
	{
		PartnerPC->StartSpectatingMode();
	}
}

void ABRGameMode::SwitchTeamToSpectatorByPlayerIndices(int32 VictimPlayerIndex, int32 PartnerPlayerIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[GameMode] 사망 후 2초 타이머 — 관전 전환 시작 (VictimIdx=%d, PartnerIdx=%d). 이 시점부터 PlayerIndex 0 반영됨."), VictimPlayerIndex, PartnerPlayerIndex);
	ABRGameState* GS = GetGameState<ABRGameState>();
	if (!GS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 관전 전환: GameState 없음"));
		return;
	}

	// Game state 역할 PlayerIndex(0=관전, 1=하체, 2=상체) 로그 헬퍼
	auto LogGameStateRolePlayerIndex = [GS](int32 PlayerIndex, const TCHAR* When) -> void
	{
		if (PlayerIndex == INDEX_NONE || !GS->PlayerArray.IsValidIndex(PlayerIndex)) return;
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(GS->PlayerArray[PlayerIndex]))
		{
			const int32 RolePlayerIndex = BRPS->bIsSpectatorSlot ? 0 : (BRPS->bIsLowerBody ? 1 : 2);
			UE_LOG(LogTemp, Log, TEXT("[GameState 역할] %s | PlayerArray Index=%d, Name=%s, bIsSpectatorSlot=%s, bIsLowerBody=%s → 역할 PlayerIndex=%d (0=관전,1=하체,2=상체)"),
				When, PlayerIndex, *BRPS->GetPlayerName(),
				BRPS->bIsSpectatorSlot ? TEXT("true") : TEXT("false"),
				BRPS->bIsLowerBody ? TEXT("true") : TEXT("false"),
				RolePlayerIndex);
		}
	};

	// 사망 관전 전환 **전** — 상체/하체 둘 다 Game state 역할 PlayerIndex 확인
	UE_LOG(LogTemp, Log, TEXT("[GameMode] 사망 관전 전환 전 — Game state 역할 PlayerIndex (0=관전, 1=하체, 2=상체)"));
	LogGameStateRolePlayerIndex(VictimPlayerIndex, TEXT("전(피해자)"));
	LogGameStateRolePlayerIndex(PartnerPlayerIndex, TEXT("전(파트너)"));

	auto TrySwitchToSpectator = [this, GS](int32 PlayerIndex) -> bool
	{
		if (PlayerIndex == INDEX_NONE || !GS->PlayerArray.IsValidIndex(PlayerIndex)) return false;
		ABRPlayerState* BRPS = Cast<ABRPlayerState>(GS->PlayerArray[PlayerIndex]);
		if (!BRPS) return false;
		ABRPlayerController* PC = Cast<ABRPlayerController>(BRPS->GetOwningController());
		// 상체 등 원격 플레이어에서 OwningController가 비어 있을 수 있음 → 월드에서 해당 PlayerState 소유 컨트롤러 검색
		if (!PC && GetWorld())
		{
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* C = It->Get();
				if (C && C->GetPlayerState<ABRPlayerState>() == BRPS)
				{
					PC = Cast<ABRPlayerController>(C);
					if (PC) break;
				}
			}
		}
		if (!PC)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameMode] 관전 전환: %s Controller 없음"), *BRPS->GetPlayerName());
			return false;
		}

		// SetSpectator(true)는 OnPlayerDied/파트너 루프에서 이미 호출됨 — 여기서는 시점 전환만
		PC->StartSpectatingMode();

		UE_LOG(LogTemp, Log, TEXT("[GameMode] 관전 전환 완료: %s (Index %d)"), *BRPS->GetPlayerName(), PlayerIndex);
		return true;
	};

	// 상체(파트너) 먼저 전환 → 시체에 붙은 상체 폰이 공격 모션 재생하는 것 방지
	UE_LOG(LogTemp, Log, TEXT("[GameMode] 관전 전환 실행: VictimIndex=%d, PartnerIndex=%d"), VictimPlayerIndex, PartnerPlayerIndex);
	TrySwitchToSpectator(PartnerPlayerIndex);
	TrySwitchToSpectator(VictimPlayerIndex);

	// 사망 관전 전환 **후** — 상체/하체 둘 다 Game state 역할 PlayerIndex가 0(관전)으로 바뀌었는지 확인
	UE_LOG(LogTemp, Log, TEXT("[GameMode] 사망 관전 전환 후 — Game state 역할 PlayerIndex (0이면 관전 반영됨)"));
	LogGameStateRolePlayerIndex(VictimPlayerIndex, TEXT("후(피해자)"));
	LogGameStateRolePlayerIndex(PartnerPlayerIndex, TEXT("후(파트너)"));

	// 승리 조건 체크: 생존 팀이 1개면 해당 팀 승리
	CheckAndEndGameIfWinner();
}

void ABRGameMode::CheckAndEndGameIfWinner()
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;

	ABRGameState* GS = GetGameState<ABRGameState>();
	if (!GS) return;

	TSet<int32> AliveTeamNumbers;
	for (APlayerState* PS : GS->PlayerArray)
	{
		ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS);
		if (!BRPS || BRPS->bIsSpectatorSlot || BRPS->TeamNumber <= 0) continue;
		if (BRPS->CurrentStatus == EPlayerStatus::Dead) continue;

		AliveTeamNumbers.Add(BRPS->TeamNumber);
	}

	UE_LOG(LogTemp, Log, TEXT("[GameMode] CheckAndEndGameIfWinner — 생존 팀 수: %d"), AliveTeamNumbers.Num());

	if (AliveTeamNumbers.Num() == 1)
	{
		const int32 WinnerTeam = AliveTeamNumbers.Array()[0];
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 승리 팀 확정 — 팀 %d 승리, EndGameWithWinner 호출"), WinnerTeam);
		GS->EndGameWithWinner(WinnerTeam);
		// 승리 확인만 함. 로비 이동은 우승 UI에서 '다음' 버튼 시 TravelToLobby() 호출로 처리 예정.
	}
}

void ABRGameMode::TravelToLobby()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[게임 종료] 로비 이동 스킵: World 없음"));
		return;
	}

	// 로비로 돌아가면 "시작한 방 제외" 플래그 해제 (다시 방 찾기 시 자신의 방이 목록에 보이도록)
	if (UBRGameInstance* GI = Cast<UBRGameInstance>(World->GetGameInstance()))
	{
		GI->SetExcludeOwnSessionFromSearch(false);
	}

	// LobbyMapPath가 블루프린트에서 비어 있으면 기본 로비 맵 사용 (BP_MainGameMode에서 미설정 시)
	static const FString DefaultLobbyMapPath = TEXT("/Game/Main/Level/Main_Scene");
	FString MapToUse = LobbyMapPath.IsEmpty() ? DefaultLobbyMapPath : LobbyMapPath;
	if (LobbyMapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[게임 종료] LobbyMapPath 비어 있음 — 기본 로비 맵 사용: %s"), *MapToUse);
	}

	ReturnToLobbyTimerHandle.Invalidate();

	const bool bIsPIE = World->IsPlayInEditor();
	const bool bShouldUseSeamlessTravel = bUseSeamlessTravel && !bIsPIE;
	const FString TravelURL = MapToUse + TEXT("?listen");

	UE_LOG(LogTemp, Warning, TEXT("[게임 종료] 로비로 이동: %s"), *MapToUse);

	if (bShouldUseSeamlessTravel)
	{
		World->ServerTravel(TravelURL, true);
	}
	else
	{
		if (bIsPIE)
		{
			const FString ServerAddr = TEXT("127.0.0.1:7777");
			const FString ClientTravelURL = ServerAddr + MapToUse;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (!PC || PC->IsLocalController()) continue;
				if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
				{
					BRPC->ClientTravelToGameMap(ClientTravelURL);
					UE_LOG(LogTemp, Log, TEXT("[게임 종료] PIE: 원격 클라이언트에게 로비 ClientTravel 전송"));
				}
			}
		}
		World->ServerTravel(TravelURL, true);
	}
}

void ABRGameMode::SwitchEliminatedTeamToSpectator(int32 EliminatedTeamNumber)
{
	ABRGameState* GS = GetGameState<ABRGameState>();
	if (!GS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 관전 전환: GameState 없음"));
		return;
	}

	int32 SwitchedCount = 0;
	for (APlayerState* OtherPS : GS->PlayerArray)
	{
		ABRPlayerState* BRPS = Cast<ABRPlayerState>(OtherPS);
		if (!BRPS || BRPS->TeamNumber != EliminatedTeamNumber) continue;

		ABRPlayerController* PC = Cast<ABRPlayerController>(BRPS->GetOwningController());
		if (!PC)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameMode] 관전 전환: %s (팀%d) Controller 없음"), *BRPS->GetPlayerName(), EliminatedTeamNumber);
			continue;
		}
		PC->StartSpectatingMode();
		SwitchedCount++;
		UE_LOG(LogTemp, Log, TEXT("[GameMode] 팀 탈락 관전 전환: %s (%s)"), *BRPS->GetPlayerName(), BRPS->bIsLowerBody ? TEXT("하체") : TEXT("상체"));
	}
	UE_LOG(LogTemp, Log, TEXT("[GameMode] 팀 %d 탈락 — 하체·상체 전원 관전 전환 완료 (%d명)"), EliminatedTeamNumber, SwitchedCount);
}
