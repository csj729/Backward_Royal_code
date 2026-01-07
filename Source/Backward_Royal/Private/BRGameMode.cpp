// BRGameMode.cpp
#include "BRGameMode.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
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

	if (ABRPlayerState* BRPS = NewPlayer->GetPlayerState<ABRPlayerState>())
	{
		if (ABRGameState* BRGameState = GetGameState<ABRGameState>())
		{
			FString PlayerName = BRPS->GetPlayerName();
			if (PlayerName.IsEmpty())
			{
				PlayerName = FString::Printf(TEXT("Player %d"), BRGameState->PlayerArray.Num());
			}
			UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] %s가 게임에 입장했습니다. (현재 인원: %d/%d)"), 
				*PlayerName, BRGameState->PlayerArray.Num(), BRGameState->MaxPlayers);
			UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] 참고: 실제 방(세션)을 만들려면 'CreateRoom [방이름]' 명령어를 사용하세요."));
			
			// 첫 번째 플레이어를 방장으로 설정
			if (BRGameState->PlayerArray.Num() == 1)
			{
				UE_LOG(LogTemp, Log, TEXT("[플레이어 입장] 첫 번째 플레이어이므로 방장으로 설정됩니다."));
				BRPS->SetIsHost(true);
			}

			// 플레이어 역할 할당 (하체/상체)
			int32 CurrentPlayerIndex = BRGameState->PlayerArray.Num() - 1; // 0부터 시작
			
			if (CurrentPlayerIndex == 0)
			{
				// 첫 번째 플레이어 = 하체
				BRPS->SetPlayerRole(true, -1); // 하체, 연결된 상체 없음
				UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: 하체 역할 할당"), *PlayerName);
			}
			else if (CurrentPlayerIndex % 2 == 0)
			{
				// 홀수 번째 플레이어 (인덱스가 짝수, 0부터 시작하므로) = 하체
				BRPS->SetPlayerRole(true, -1); // 하체, 연결된 상체 없음
				UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: 하체 역할 할당"), *PlayerName);
			}
			else
			{
				// 짝수 번째 플레이어 = 이전 플레이어의 상체
				int32 LowerBodyPlayerIndex = CurrentPlayerIndex - 1;
				if (LowerBodyPlayerIndex >= 0 && LowerBodyPlayerIndex < BRGameState->PlayerArray.Num())
				{
					if (ABRPlayerState* LowerBodyPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[LowerBodyPlayerIndex]))
					{
						// 상체 역할 할당
						BRPS->SetPlayerRole(false, LowerBodyPlayerIndex);
						// 하체 플레이어의 연결된 상체 인덱스 업데이트
						LowerBodyPS->SetPlayerRole(true, CurrentPlayerIndex);
						UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: 상체 역할 할당 (하체 플레이어 인덱스: %d)"), 
							*PlayerName, LowerBodyPlayerIndex);
					}
				}
			}
		}
	}

	// 플레이어 목록 업데이트
	if (ABRGameState* BRGameState = GetGameState<ABRGameState>())
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
	
	// 맵 이동 - Seamless Travel 사용 (더 부드러운 전환)
	FString TravelURL = GameMapPath + TEXT("?listen");
	UE_LOG(LogTemp, Log, TEXT("[게임 시작] SeamlessTravel 호출: %s"), *TravelURL);
	
	// SeamlessTravel 사용 - 클라이언트가 부드럽게 따라옵니다
	// bUseSeamlessTravel = true이므로 SeamlessTravel을 사용할 수 있습니다
	if (bUseSeamlessTravel)
	{
		// SeamlessTravel은 GameMode의 함수가 아니라 World의 함수입니다
		// 하지만 ServerTravel을 사용하되, bUseSeamlessTravel이 true이면 자동으로 Seamless Travel을 사용합니다
		World->ServerTravel(TravelURL, true); // 두 번째 파라미터는 bAbsolute (true = 절대 경로)
	}
	else
	{
		World->ServerTravel(TravelURL);
	}
}

