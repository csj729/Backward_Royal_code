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
			FTimerHandle H;
			GetWorld()->GetTimerManager().SetTimer(H, this, &ABRGameMode::ApplyRoleChangesForRandomTeams, 1.5f, false);
			UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 게임 맵 로드됨 - 1.5초 후 상체/하체 Pawn 적용 예정"));
		}
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
		// [보존] 플레이어 이름 설정 및 로그
		FString PlayerName = BRPS->GetPlayerName();
		UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin 진입 | bIsLocalPlayer 판단 전 | GetPlayerName()='%s'"), *PlayerName);

		if (PlayerName.IsEmpty())
		{
			PlayerName = FString::Printf(TEXT("Player %d"), BRGameState->PlayerArray.Num());
			UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | PlayerName 비어있어 기본값 사용: '%s'"), *PlayerName);
		}

		// GameInstance 이름은 이 머신의 로컬 플레이어(호스트/Standalone)일 때만 적용.
		// 클라이언트 입장 시 서버의 GI는 호스트 이름이므로, 원격 클라이언트에 호스트 이름을 덮어쓰지 않음.
		UWorld* WorldForCheck = GetWorld();
		const bool bIsLocalPlayer = WorldForCheck && (WorldForCheck->GetNetMode() == NM_Standalone || NewPlayer->IsLocalController());
		UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance());
		UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | bIsLocalPlayer=%d | NetMode=%d | IsLocalController=%d"),
			bIsLocalPlayer ? 1 : 0, WorldForCheck ? (int32)WorldForCheck->GetNetMode() : -1, NewPlayer->IsLocalController() ? 1 : 0);

		if (bIsLocalPlayer && GI)
		{
			FString SavedPlayerName = GI->GetPlayerName();
			UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | 로컬 플레이어 → GI->GetPlayerName()='%s'"), *SavedPlayerName);
			if (!SavedPlayerName.IsEmpty())
			{
				PlayerName = SavedPlayerName;
				BRPS->SetPlayerName(PlayerName);
				UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | 로컬: BRPS->SetPlayerName('%s') 적용"), *PlayerName);
			}
		}
		// 원격 클라이언트: 이름을 "Player N"으로 덮어쓰지 않음. 빈/PC이름이면 그대로 두고 ServerSetPlayerName 도착 시 실제 이름으로 갱신 → UI는 공란 후 PlayerName 표시
		if (!bIsLocalPlayer)
		{
			const FString CurrentName = BRPS->GetPlayerName();
			UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | 원격 클라이언트: GetPlayerName() 유지 '%s' (ServerSetPlayerName 도착 시 갱신)"), *CurrentName);
		}

		// UserUID 설정 (GameInstance에서 가져오거나 생성)
		if (GI)
		{
			// UserUID가 비어있으면 자동 생성 (예: Steam ID, 계정 ID 등)
			FString UserUID = FString::Printf(TEXT("Player_%d_%s"), 
				BRGameState->PlayerArray.Num() - 1, 
				*FDateTime::Now().ToString());
			BRPS->SetUserUID(UserUID);
			UE_LOG(LogTemp, Warning, TEXT("[로비이름] PostLogin | SetUserUID('%s') | 최종 PlayerName='%s'"), *UserUID, *BRPS->GetPlayerName());
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

		// [보존] 방장 설정 — ListenServer/DedicatedServer에서만 적용 (Standalone은 방 생성 버튼 누를 때만 방 생성)
		if (BRGameState->PlayerArray.Num() == 1 && (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer))
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

		// [보존] 플레이어 역할 할당 로직
		int32 CurrentPlayerIndex = BRGameState->PlayerArray.Num() - 1;

		if (CurrentPlayerIndex == 0 || CurrentPlayerIndex % 2 == 0)
		{
			// [하체]
			BRPS->SetPlayerRole(true, -1);
			UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: 하체 역할 할당"), *PlayerName);

			if (APawn* NewPawn = NewPlayer->GetPawn())
			{
				NewPawn->SetOwner(NewPlayer); // RPC 권한 부여
			}
		}
		else
		{
			// [상체]
			int32 LowerBodyPlayerIndex = CurrentPlayerIndex - 1;
			if (BRGameState->PlayerArray.IsValidIndex(LowerBodyPlayerIndex))
			{
				if (ABRPlayerState* LowerBodyPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[LowerBodyPlayerIndex]))
				{
					// NewPlayer가 처음 접속하며 자동으로 배정받은 기본 Pawn을 가져옵니다.
					APawn* ProxyPawn = NewPlayer->GetPawn();
					if (ProxyPawn)
					{
						UE_LOG(LogTemp, Warning, TEXT("[시스템] %s의 기존 Proxy Pawn(%s)을 삭제합니다."), *PlayerName, *ProxyPawn->GetName());
						ProxyPawn->Destroy();
					}

					// 역할 데이터 업데이트
					BRPS->SetPlayerRole(false, LowerBodyPlayerIndex);
					LowerBodyPS->SetPlayerRole(true, CurrentPlayerIndex);

					UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: 상체 역할 할당 (하체 플레이어 인덱스: %d)"),
						*PlayerName, LowerBodyPlayerIndex);

					// -----------------------------------------------------------
					// [추가 기능] 서버 권한 상체 스폰 및 소유권 부여
					// -----------------------------------------------------------
					APlayerController* LowerBodyController = Cast<APlayerController>(LowerBodyPS->GetOwningController());
					if (LowerBodyController && UpperBodyClass)
					{
						APlayerCharacter* LowerChar = Cast<APlayerCharacter>(LowerBodyController->GetPawn());
						if (LowerChar)
						{
							FActorSpawnParameters SpawnParams;
							SpawnParams.Owner = NewPlayer; // RPC 권한을 위한 소유자 설정
							SpawnParams.Instigator = NewPlayer->GetPawn();
							SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

							AUpperBodyPawn* NewUpper = GetWorld()->SpawnActor<AUpperBodyPawn>(
								UpperBodyClass, LowerChar->GetActorLocation(), LowerChar->GetActorRotation(), SpawnParams
							);

							if (NewUpper)
							{
								// 물리적 부착
								NewUpper->AttachToComponent(
									LowerChar->HeadMountPoint,
									FAttachmentTransformRules::SnapToTargetNotIncludingScale
								);

								// 상호 참조 연결
								NewUpper->ParentBodyCharacter = LowerChar;
								LowerChar->SetUpperBodyPawn(NewUpper);

								// 상체 조종자가 이 Pawn을 직접 제어하도록 빙의
								NewPlayer->Possess(NewUpper);

								UE_LOG(LogTemp, Log, TEXT("[서버 생성] %s의 상체 Pawn이 생성되어 하체(Index %d)에 부착되었습니다."), *PlayerName, LowerBodyPlayerIndex);
							}
						}
					}
				}
			}
		}
	}

	// [보존] 플레이어 목록 업데이트
	if (BRGameState)
	{
		BRGameState->UpdatePlayerList();
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
	if (!HasAuthority() || !UpperBodyClass) return;

	UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance());
	if (!GI || !GI->GetPendingApplyRandomTeamRoles())
		return;
	GI->ClearPendingApplyRandomTeamRoles();

	UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] ApplyRoleChangesForRandomTeams 진입 (1.5초 타이머)"));

	ABRGameState* BRGameState = GetGameState<ABRGameState>();
	if (!BRGameState || BRGameState->PlayerArray.Num() < 2) return;

	// Seamless Travel 후 PlayerState가 초기화될 수 있으므로, 저장해 둔 팀/역할을 복원
	GI->RestorePendingRolesFromTravel(BRGameState);

	UWorld* World = GetWorld();
	if (!World) return;

	// 팀 순서 + 팀 내 하체 먼저: (TeamNumber, bIsLowerBody?0:1) 으로 정렬
	TArray<ABRPlayerState*> SortedByTeam;
	for (APlayerState* PS : BRGameState->PlayerArray)
	{
		if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(PS))
			SortedByTeam.Add(BRPS);
	}
	Algo::Sort(SortedByTeam, [](const ABRPlayerState* A, const ABRPlayerState* B)
	{
		if (A->TeamNumber != B->TeamNumber) return A->TeamNumber < B->TeamNumber;
		return A->bIsLowerBody && !B->bIsLowerBody; // 하체 먼저
	});

	const int32 NumPlayers = SortedByTeam.Num();
	const int32 NumTeams = NumPlayers / 2;
	if (NumTeams < 1) return;

	// 상체로 지정된 플레이어들의 기존(하체) Pawn만 먼저 제거 → 팀당 1개 하체 몸통만 남김
	for (int32 TeamIndex = 0; TeamIndex < NumTeams; TeamIndex++)
	{
		ABRPlayerState* UpperPS = SortedByTeam[2 * TeamIndex + 1];
		if (UpperPS && !UpperPS->bIsLowerBody) // 상체인 경우만
		{
			// PlayerState → Controller는 GetOwningController() 사용 (GetOwner()는 PlayerState에서 설정되지 않을 수 있음)
			APlayerController* UpperPC = Cast<APlayerController>(UpperPS->GetOwningController());
			if (UpperPC)
			{
				APawn* OldPawn = UpperPC->GetPawn();
				UpperPC->UnPossess();
				if (OldPawn && IsValid(OldPawn))
					OldPawn->Destroy();
			}
		}
	}

	for (int32 TeamIndex = 0; TeamIndex < NumTeams; TeamIndex++)
	{
		ABRPlayerState* LowerPS = SortedByTeam[2 * TeamIndex];
		ABRPlayerState* UpperPS = SortedByTeam[2 * TeamIndex + 1];
		// 역할이 서버에 올바르게 적용되었는지 확인 (하체→상체 순서)
		if (LowerPS->bIsLowerBody == false || UpperPS->bIsLowerBody == true)
		{
			UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 팀 %d 역할 불일치 - 하체:%s 상체:%s, 스킵"), TeamIndex + 1,
				LowerPS->bIsLowerBody ? TEXT("Y") : TEXT("N"), UpperPS->bIsLowerBody ? TEXT("Y") : TEXT("N"));
			continue;
		}
		// PlayerState → Controller는 GetOwningController() 사용 (GetOwner()는 null일 수 있어 2팀 상체 등 빙의 실패 원인)
		APlayerController* LowerPC = Cast<APlayerController>(LowerPS->GetOwningController());
		APlayerController* UpperPC = Cast<APlayerController>(UpperPS->GetOwningController());
		if (!LowerPC || !UpperPC)
		{
			UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 팀 %d Controller 없음 - 하체PC:%s 상체PC:%s, 스킵"),
				TeamIndex + 1, LowerPC ? TEXT("O") : TEXT("X"), UpperPC ? TEXT("O") : TEXT("X"));
			continue;
		}

		// 하체 플레이어가 현재 소유한 Pawn 사용 (가입 순서가 아닌 역할 기준)
		APlayerCharacter* LowerChar = Cast<APlayerCharacter>(LowerPC->GetPawn());
		if (!LowerChar || !IsValid(LowerChar))
		{
			UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 팀 %d 하체 플레이어 %s의 Pawn 없음, 스킵"), TeamIndex + 1, *LowerPS->GetPlayerName());
			continue;
		}

		// 하체가 해당 LowerChar를 소유하도록
		if (LowerPC->GetPawn() != LowerChar)
		{
			LowerPC->UnPossess();
			LowerPC->Possess(LowerChar);
		}

		// 상체가 갖고 있던 기존 Pawn 제거
		APawn* OldUpperPawn = UpperPC->GetPawn();
		UpperPC->UnPossess();
		if (OldUpperPawn)
			OldUpperPawn->Destroy();

		// 이 하체에 붙어 있던 기존 상체 Pawn 제거 (이터레이터 중 Destroy 방지를 위해 수집 후 제거)
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

		// 상체 Pawn 스폰 및 부착·빙의
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
			UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] 팀 %d: %s 상체 스폰 후 빙의"), TeamIndex + 1, *UpperPS->GetPlayerName());
		}
	}
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

	// 맵 선택: 랜덤 맵 사용 여부에 따라 결정
	FString SelectedMapPath;
	if (bUseRandomMap && StageMapPaths.Num() > 0)
	{
		// 랜덤으로 Stage 맵 중 하나 선택
		int32 RandomIndex = FMath::RandRange(0, StageMapPaths.Num() - 1);
		SelectedMapPath = StageMapPaths[RandomIndex];
		UE_LOG(LogTemp, Log, TEXT("[게임 시작] 랜덤 맵 선택: %s (인덱스: %d/%d)"), 
			*SelectedMapPath, RandomIndex + 1, StageMapPaths.Num());
	}
	else
	{
		// 기존 GameMapPath 사용
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
						// 클라이언트에게 게임 시작 알림 (RPC)
						BRPC->ClientNotifyGameStarting();
					}
				}
			}
			
			// PIE 환경에서는 ServerTravel이 클라이언트를 자동으로 따라오지 않을 수 있음
			// 하지만 일반적으로 ServerTravel은 클라이언트가 자동으로 따라옵니다
			// 서버(호스트)는 ServerTravel 사용 - 클라이언트가 자동으로 따라옵니다
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

	// 캐릭터에서 PlayerController 및 PlayerState 가져오기
	if (AController* Controller = VictimCharacter->GetController())
	{
		if (ABRPlayerState* PS = Controller->GetPlayerState<ABRPlayerState>())
		{
			// PlayerState에 사망 상태가 아직 반영 안 되었다면 여기서 확실히 처리
			if (PS->CurrentStatus != EPlayerStatus::Dead)
			{
				PS->SetPlayerStatus(EPlayerStatus::Dead);
			}

			UE_LOG(LogTemp, Log, TEXT("[GameMode] %s (Team %d) 탈락 처리 완료"),
				*PS->GetPlayerName(), PS->TeamNumber);
		}
	}

	// TODO: 여기에 남은 생존 팀 수를 확인하여 '게임 종료(우승)' 판정 로직 추가
	// 예: CheckGameEndCondition();
}