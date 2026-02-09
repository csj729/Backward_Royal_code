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

ABRGameMode::ABRGameMode()
{
	// GameState 클래스 설정
	GameStateClass = ABRGameState::StaticClass();
	PlayerStateClass = ABRPlayerState::StaticClass();
	PlayerControllerClass = ABRPlayerController::StaticClass();
	GameSessionClass = ABRGameSession::StaticClass();

	// 리슨 서버 설정
	bUseSeamlessTravel = true;
}

void ABRGameMode::ClearGameSessionForPIEExit()
{
	GameSession = nullptr;
}

TArray<FString> ABRGameMode::GetAvailableStageMapPaths() const
{
	TArray<FString> Result;
	if (StageFolderPath.IsEmpty())
	{
		return StageMapPathsFallback;
	}

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage 맵] Asset Registry를 사용할 수 없어 폴백 목록을 사용합니다."));
		return StageMapPathsFallback;
	}

	TArray<FAssetData> AssetDataList;
	const FName PackagePath(*StageFolderPath);
	AssetRegistry->GetAssetsByPath(PackagePath, AssetDataList, true, false);

	// UE5: 월드(맵) 에셋만 필터 (클래스 경로 /Script/Engine.World)
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
		UE_LOG(LogTemp, Log, TEXT("[Stage 맵] 폴더에서 %d개 맵 수집: %s"), Result.Num(), *StageFolderPath);
		return Result;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Stage 맵] 폴더에서 맵을 찾지 못해 폴백 목록을 사용합니다. (폴더: %s)"), *StageFolderPath);
	return StageMapPathsFallback;
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
			ScheduleInitialRoleApplyIfNeeded();
	}
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
	if (!BRGameState || BRGameState->PlayerArray.Num() < 2)
		return;

	// 저장된 인원 수만큼 플레이어가 다 들어올 때까지 짧게 대기 (전원 하체로 나오는 현상 방지)
	const int32 ExpectedCount = GI->GetPendingRoleRestoreCount();
	if (ExpectedCount > 0 && BRGameState->PlayerArray.Num() < ExpectedCount)
	{
		FTimerHandle H;
		GetWorld()->GetTimerManager().SetTimer(H, this, &ABRGameMode::ApplyRoleChangesForRandomTeams, 0.5f, false);
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 플레이어 대기 중 (%d/%d), 0.5초 후 재시도"), BRGameState->PlayerArray.Num(), ExpectedCount);
		return;
	}

	// 플래그는 하체 Pawn 확인 통과 후에만 클리어 (Pawn 없이 재시도할 때 플래그 유지)
	UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] ApplyRoleChangesForRandomTeams 진입 (상체/하체 Pawn 적용)"));

	// Seamless Travel 후 PlayerState가 초기화될 수 있으므로, 저장해 둔 팀/역할을 복원
	GI->RestorePendingRolesFromTravel(BRGameState);

	UWorld* World = GetWorld();
	if (!World) return;

	// 관전자 제외, 팀 순서 + 팀 내 하체 먼저로 정렬 (팀당 하체/상체만 스폰)
	TArray<ABRPlayerState*> SortedByTeam;
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

	const int32 NumPlayers = SortedByTeam.Num();
	const int32 NumTeams = NumPlayers / 2;
	if (NumTeams < 1)
	{
		GI->ClearPendingApplyRandomTeamRoles();
		GI->ClearPendingRoleRestoreData();
		return;
	}

	// 순차 스폰: 1팀 하체 확인 → 1팀 상체 스폰 → 2팀 하체 확인 → 2팀 상체 스폰 … (앞사람이 전부 정상 스폰된 뒤 다음으로 진행)
	// 기존 순차 스폰 타이머가 있으면 취소
	World->GetTimerManager().ClearTimer(StagedApplyTimerHandle);
	StagedSortedByTeam = SortedByTeam;
	StagedNumTeams = NumTeams;
	StagedCurrentTeamIndex = 0;
	StagedPawnWaitRetriesForTeam = 0;
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
		StagedSortedByTeam.Empty();
		StagedNumTeams = 0;
		StagedCurrentTeamIndex = 0;
		StagedPawnWaitRetriesForTeam = 0;
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 순차 상체 스폰 완료"));
		return;
	}

	const int32 TeamIndex = StagedCurrentTeamIndex;
	ABRPlayerState* LowerPS = StagedSortedByTeam.IsValidIndex(2 * TeamIndex) ? StagedSortedByTeam[2 * TeamIndex] : nullptr;
	ABRPlayerState* UpperPS = StagedSortedByTeam.IsValidIndex(2 * TeamIndex + 1) ? StagedSortedByTeam[2 * TeamIndex + 1] : nullptr;

	if (!LowerPS || !UpperPS)
	{
		StagedPawnWaitRetriesForTeam = 0;
		StagedCurrentTeamIndex++;
		World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, FMath::Max(0.01f, SpawnDelayBetweenTeams), false);
		return;
	}

	if (LowerPS->bIsLowerBody == false || UpperPS->bIsLowerBody == true)
	{
		UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 팀 %d 역할 불일치 - 스킵"), TeamIndex + 1);
		StagedPawnWaitRetriesForTeam = 0;
		StagedCurrentTeamIndex++;
		World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, FMath::Max(0.01f, SpawnDelayBetweenTeams), false);
		return;
	}

	APlayerController* LowerPC = Cast<APlayerController>(LowerPS->GetOwningController());
	APlayerController* UpperPC = Cast<APlayerController>(UpperPS->GetOwningController());
	if (!LowerPC || !UpperPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 팀 %d Controller 없음, 스킵"), TeamIndex + 1);
		StagedPawnWaitRetriesForTeam = 0;
		StagedCurrentTeamIndex++;
		World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, FMath::Max(0.01f, SpawnDelayBetweenTeams), false);
		return;
	}

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
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 팀 %d: %s 상체 스폰 후 빙의 (순차 %d/%d)"), TeamIndex + 1, *UpperPS->GetPlayerName(), TeamIndex + 1, StagedNumTeams);
	}

	StagedCurrentTeamIndex++;
	if (StagedCurrentTeamIndex < StagedNumTeams)
		World->GetTimerManager().SetTimer(StagedApplyTimerHandle, this, &ABRGameMode::ApplyRoleChangesForRandomTeams_ApplyOneTeam, FMath::Max(0.01f, SpawnDelayBetweenTeams), false);
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
	Super::EndPlay(EndPlayReason);
	
	// PIE 종료 시 NavigationSystem이 World를 참조하여 GC가 되지 않는 문제는
	// Unreal Engine의 알려진 버그입니다. NavigationSystem은 World가 파괴될 때
	// 자동으로 정리되어야 하지만, PIE 종료 시 타이밍 문제로 인해 참조가 남을 수 있습니다.
	// 
	// 참고: NavigationSystem을 직접 정리하는 것은 권장되지 않으며,
	// World의 정리 순서에 문제가 있을 수 있습니다.
	// 이 문제는 주로 비동기 네비게이션 메시 빌드가 진행 중일 때 발생합니다.
	
	UWorld* World = GetWorld();
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

	ABRGameState* GS = GetGameState<ABRGameState>();
	if (!GS) return;

	// 캐릭터에서 PlayerController 및 PlayerState 가져오기
	AController* Controller = VictimCharacter->GetController();
	if (!Controller) return;

	ABRPlayerState* PS = Controller->GetPlayerState<ABRPlayerState>();
	if (!PS) return;

	// 1) 사망한 플레이어 → Dead + 관전(PlayerIndex 0)
	if (PS->CurrentStatus != EPlayerStatus::Dead)
	{
		PS->SetPlayerStatus(EPlayerStatus::Dead);
	}
	PS->SetSpectator(true);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] %s (Team %d) 탈락 → 관전(PlayerIndex 0)으로 전환"),
		*PS->GetPlayerName(), PS->TeamNumber);

	// 2) 같은 팀 파트너(상체/하체)도 관전(PlayerIndex 0)으로 전환 (한 몸이므로 둘 다 탈락)
	if (PS->ConnectedPlayerIndex >= 0 && GS->PlayerArray.IsValidIndex(PS->ConnectedPlayerIndex))
	{
		if (ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(GS->PlayerArray[PS->ConnectedPlayerIndex]))
		{
			if (PartnerPS->CurrentStatus != EPlayerStatus::Dead)
			{
				PartnerPS->SetPlayerStatus(EPlayerStatus::Spectating);
			}
			PartnerPS->SetSpectator(true);
			UE_LOG(LogTemp, Log, TEXT("[GameMode] 파트너 %s (Team %d) → 관전(PlayerIndex 0)으로 전환"),
				*PartnerPS->GetPlayerName(), PartnerPS->TeamNumber);
		}
	}

	// TODO: 여기에 남은 생존 팀 수를 확인하여 '게임 종료(우승)' 판정 로직 추가
	// 예: CheckGameEndCondition();
}