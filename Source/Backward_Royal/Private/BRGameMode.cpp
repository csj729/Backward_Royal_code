// BRGameMode.cpp
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "BRGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "UpperBodyPawn.h"
#include "PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"

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
		if (PlayerName.IsEmpty())
		{
			PlayerName = FString::Printf(TEXT("Player %d"), BRGameState->PlayerArray.Num());
		}

		// GameInstance에서 플레이어 이름 가져오기
		if (UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance()))
		{
			FString SavedPlayerName = GI->GetPlayerName();
			if (!SavedPlayerName.IsEmpty())
			{
				PlayerName = SavedPlayerName;
				BRPS->SetPlayerName(PlayerName);
			}
		}

		// UserUID 설정 (GameInstance에서 가져오거나 생성)
		if (UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance()))
		{
			// UserUID가 비어있으면 자동 생성 (예: Steam ID, 계정 ID 등)
			FString UserUID = FString::Printf(TEXT("Player_%d_%s"), 
				BRGameState->PlayerArray.Num() - 1, 
				*FDateTime::Now().ToString());
			BRPS->SetUserUID(UserUID);
		}

		UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] %s가 게임에 입장했습니다. (현재 인원: %d/%d)"),
			*PlayerName, BRGameState->PlayerArray.Num(), BRGameState->MaxPlayers);
		UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] 참고: 실제 방(세션)을 만들려면 'CreateRoom [방이름]' 명령어를 사용하세요."));

		// [보존] 방장 설정
		if (BRGameState->PlayerArray.Num() == 1)
		{
			UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] 첫 번째 플레이어이므로 방장으로 설정됩니다."));
			BRPS->SetIsHost(true);
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
					APlayerController* LowerBodyController = Cast<APlayerController>(LowerBodyPS->GetOwner());
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

	Super::Logout(Exiting);

	// 플레이어 목록 업데이트 및 역할 재할당
	if (ABRGameState* BRGameState = GetGameState<ABRGameState>())
	{
		BRGameState->UpdatePlayerList();
		
		// 남은 플레이어들의 역할 재할당 (순서대로)
		for (int32 i = 0; i < BRGameState->PlayerArray.Num(); i++)
		{
			if (ABRPlayerState* BRPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[i]))
			{
				if (i == 0)
				{
					// 첫 번째 플레이어 = 하체
					BRPS->SetPlayerRole(true, -1);
				}
				else if (i % 2 == 0)
				{
					// 홀수 번째 플레이어 (인덱스가 짝수) = 하체
					BRPS->SetPlayerRole(true, -1);
				}
				else
				{
					// 짝수 번째 플레이어 = 이전 플레이어의 상체
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

	UE_LOG(LogTemp, Log, TEXT("[게임 시작] 성공: 맵으로 이동 중... (%s)"), *GameMapPath);
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[게임 시작] 실패: World를 찾을 수 없습니다."));
		return;
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
	FString TravelURL = GameMapPath + TEXT("?listen");
	
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

