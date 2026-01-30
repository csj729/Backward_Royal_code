// BRPlayerController.cpp
#include "BRPlayerController.h"
#include "BRCheatManager.h"
#include "BRGameSession.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "BRGameMode.h"
#include "BRGameInstance.h"
#include "UpperBodyPawn.h"
#include "GameFramework/GameModeBase.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"

ABRPlayerController::ABRPlayerController()
	: CurrentMenuWidget(nullptr)
	, MainScreenWidget(nullptr)
{
	// CheatManager 클래스 설정
	CheatClass = UBRCheatManager::StaticClass();
}

void ABRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (CheatManager)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] CheatManager 초기화 완료 - 콘솔 명령어 사용 가능"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] CheatManager가 초기화되지 않았습니다."));
	}
	
	// 네트워크 연결 실패 델리게이트 바인딩
	if (UEngine* Engine = GEngine)
	{
		Engine->OnNetworkFailure().AddUObject(this, &ABRPlayerController::HandleNetworkFailure);
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] 네트워크 실패 감지 델리게이트 바인딩 완료"));
	}

	// GameSession 이벤트 바인딩 (방 생성 완료)
	if (UWorld* World = GetWorld())
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
			{
				GameSession->OnCreateSessionComplete.AddDynamic(this, &ABRPlayerController::HandleCreateRoomComplete);
				UE_LOG(LogTemp, Log, TEXT("[PlayerController] GameSession 이벤트 바인딩 완료"));
			}
		}
	}

	// 클라이언트에서만 초기 UI 표시
	// PostLogin 완료 후 로비 판단하려면 짧은 지연 필요. EndPlay에서 타이머 해제 + 람다 내 IsValid 검사로 open ?listen 크래시 방지.
	if (IsLocalController())
	{
		UWorld* World = GetWorld();
		if (!World) return;
		World->GetTimerManager().SetTimer(BeginPlayUITimerHandle, [this]()
		{
			// 맵 전환(open ?listen 등)으로 파괴된 뒤 콜백 방지
			if (!IsValid(this))
			{
				return;
			}
			UWorld* W = GetWorld();
			if (!W)
			{
				return;
			}

			W->GetTimerManager().ClearTimer(BeginPlayUITimerHandle);

			ENetMode NetMode = W->GetNetMode();
			// 클라이언트 입장 확인 (Standalone 모드에서도 확인)
			if (NetMode == NM_Client)
			{
				UE_LOG(LogTemp, Warning, TEXT("========================================"));
				UE_LOG(LogTemp, Warning, TEXT("[클라이언트] 서버 연결 확인!"));
				UE_LOG(LogTemp, Warning, TEXT("========================================"));
				
				// 네트워크 연결 상태 확인
				if (UNetDriver* NetDriver = W->GetNetDriver())
				{
					if (UNetConnection* ServerConnection = NetDriver->ServerConnection)
					{
						FString RemoteAddress = ServerConnection->LowLevelGetRemoteAddress(true);
						UE_LOG(LogTemp, Warning, TEXT("[클라이언트] 서버 주소: %s"), *RemoteAddress);
						UE_LOG(LogTemp, Warning, TEXT("[클라이언트] 연결 상태: 연결됨"));
						
						if (GEngine)
						{
							FString ConnectMsg = FString::Printf(TEXT("[클라이언트] 서버 연결 성공!\n주소: %s"), *RemoteAddress);
							GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, ConnectMsg);
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("[클라이언트] ServerConnection이 NULL입니다!"));
						if (GEngine)
						{
							GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[클라이언트] 서버 연결 실패!"));
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[클라이언트] NetDriver가 NULL입니다!"));
				}
				
				// GameState 확인 (서버 데이터 복제 확인)
				if (ABRGameState* BRGameState = W->GetGameState<ABRGameState>())
				{
					UE_LOG(LogTemp, Warning, TEXT("[클라이언트] GameState 확인: 현재 인원 %d"), BRGameState->PlayerArray.Num());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[클라이언트] GameState가 아직 복제되지 않았습니다. 잠시 후 확인하세요."));
				}
			}
			
			// 세션이 활성화되어 있는지 확인 (ServerTravel 후 맵 재로드 시 세션이 있을 수 있음)
			bool bHasActiveSession = false;
			if (AGameModeBase* GameMode = W->GetAuthGameMode())
			{
				if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
				{
					bHasActiveSession = GameSession->HasActiveSession();
				}
			}
			
			// 플레이어가 이미 입장했다면 (PostLogin 호출됨) 세션이 있다고 간주
			// ServerTravel 후 세션이 일시적으로 사라질 수 있지만, 플레이어 입장은 유지됨
			bool bHasPlayers = false;
			int32 PlayerCount = 0;
			if (ABRGameState* BRGameState = W->GetGameState<ABRGameState>())
			{
				PlayerCount = BRGameState->PlayerArray.Num();
				bHasPlayers = PlayerCount > 0;
			}
			
			// 방 생성 후 ServerTravel 직전에 설정된 플래그 (GameInstance 유지)
			bool bDidCreateRoomThenTravel = false;
			if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(W->GetGameInstance()))
			{
				bDidCreateRoomThenTravel = BRGI->GetDidCreateRoomThenTravel();
			}
			
			// 세션이 있거나, (방 생성→ServerTravel 직후이며 플레이어 있음) 이면 방 생성 완료 상태로 간주
			// Standalone 모드에서는 항상 main UI를 표시하므로 bRoomCreated를 false로 설정
			bool bRoomCreated = false;
			if (NetMode != NM_Standalone)
			{
				// Standalone이 아닌 경우에만 방 생성 상태 확인
				bRoomCreated = bHasActiveSession || (bDidCreateRoomThenTravel && bHasPlayers);
			}
			
			if (GEngine)
			{
				FString DebugMsg = FString::Printf(TEXT("[BeginPlay] NetMode: %s, HasActiveSession: %s, Players: %d, bDidCreateRoomThenTravel: %s"), 
					NetMode == NM_Standalone ? TEXT("Standalone") :
					NetMode == NM_ListenServer ? TEXT("ListenServer") :
					NetMode == NM_Client ? TEXT("Client") : TEXT("Other"),
					bHasActiveSession ? TEXT("Yes") : TEXT("No"),
					PlayerCount,
					bDidCreateRoomThenTravel ? TEXT("Yes") : TEXT("No"));
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugMsg);
			}
			
			UE_LOG(LogTemp, Warning, TEXT("[PlayerController] BeginPlay UI 결정: NetMode=%s, bHasActiveSession=%s, bHasPlayers=%s, bDidCreateRoomThenTravel=%s"),
				NetMode == NM_Standalone ? TEXT("Standalone") :
				NetMode == NM_ListenServer ? TEXT("ListenServer") :
				NetMode == NM_Client ? TEXT("Client") : TEXT("Other"),
				bHasActiveSession ? TEXT("Yes") : TEXT("No"),
				bHasPlayers ? TEXT("Yes") : TEXT("No"),
				bDidCreateRoomThenTravel ? TEXT("Yes") : TEXT("No"));
			
			// Standalone 모드에서는 항상 main UI (EntranceMenu)를 표시
			// NetMode를 변경하지 않고 실제 NetMode에 따라 UI를 결정
			if (bRoomCreated && NetMode == NM_ListenServer)
			{
				// ListenServer 모드에서 세션이 활성화되어 있으면 정상적인 방 생성 완료 상태
				UE_LOG(LogTemp, Log, TEXT("[PlayerController] ListenServer 모드에서 세션/플레이어 확인 - 로비로 이동"));
			}
			
			// 로비 표시 시 플래그 클리어 (다음 메인 복귀 시 엔트런스 표시용)
			if ((NetMode == NM_Client || NetMode == NM_ListenServer) && (MainScreenWidget || LobbyMenuWidgetClass))
			{
				if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(W->GetGameInstance()))
				{
					BRGI->SetDidCreateRoomThenTravel(false);
				}
			}
			
			// MainScreenWidget이 설정되어 있으면 네트워크 모드에 따라 적절한 메뉴로 전환
			if (MainScreenWidget && IsValid(MainScreenWidget))
			{
				// Standalone 모드 처리
				if (NetMode == NM_Standalone)
				{
					// 방 생성 후 ServerTravel로 인한 재로드인 경우 LobbyMenu 표시
					// 세션이 없어도 플레이어가 있고 방 생성 플래그가 있으면 LobbyMenu 표시
					// (ServerTravel 직후 NetMode가 아직 Standalone일 수 있지만, 방 생성 후 재로드 상태면 LobbyMenu)
					// 세션 체크는 제거 - ServerTravel 직후 세션이 아직 초기화되지 않을 수 있음
					bool bShouldShowLobby = bDidCreateRoomThenTravel && bHasPlayers;
					
					UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Standalone 모드 UI 결정: bShouldShowLobby=%s (bDidCreateRoomThenTravel=%s, bHasPlayers=%s)"),
						bShouldShowLobby ? TEXT("Yes") : TEXT("No"),
						bDidCreateRoomThenTravel ? TEXT("Yes") : TEXT("No"),
						bHasPlayers ? TEXT("Yes") : TEXT("No"));
					
					if (bShouldShowLobby)
					{
						// 방 생성 후 재로드 상태 - LobbyMenu 표시
						SetMainScreenToLobbyMenu();
						UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 초기 UI (LobbyMenu) 표시 - Standalone 모드이지만 방 생성 후 재로드 상태"));
					}
					else
					{
						// 일반 Standalone 모드 - EntranceMenu 표시
						if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(W->GetGameInstance()))
						{
							BRGI->SetDidCreateRoomThenTravel(false);
						}
						SetMainScreenToEntranceMenu();
						UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 초기 UI (EntranceMenu) 표시 - Standalone 모드 (강제)"));
					}
				}
				else if (NetMode == NM_Client)
				{
					// Client 모드는 항상 LobbyMenu (서버에 연결된 상태)
					SetMainScreenToLobbyMenu();
					UE_LOG(LogTemp, Log, TEXT("[PlayerController] 초기 UI (LobbyMenu) 표시 - Client 모드"));
				}
				else if (NetMode == NM_ListenServer)
				{
					// ListenServer 모드: 세션이 있으면 LobbyMenu, 없으면 MainMenu
					if (bHasActiveSession || (bDidCreateRoomThenTravel && bHasPlayers))
					{
						// 세션이 있거나 방 생성 후 재로드 상태 - LobbyMenu 표시
						SetMainScreenToLobbyMenu();
						UE_LOG(LogTemp, Log, TEXT("[PlayerController] 초기 UI (LobbyMenu) 표시 - ListenServer 모드 (세션 있음)"));
					}
					else
					{
						// 세션이 없음 - MainMenu 표시 (처음 실행 시)
						if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(W->GetGameInstance()))
						{
							BRGI->SetDidCreateRoomThenTravel(false);
						}
						SetMainScreenToEntranceMenu();
						UE_LOG(LogTemp, Log, TEXT("[PlayerController] 초기 UI (EntranceMenu) 표시 - ListenServer 모드 (세션 없음)"));
					}
				}
				else
				{
					if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(W->GetGameInstance()))
					{
						BRGI->SetDidCreateRoomThenTravel(false);
					}
					SetMainScreenToEntranceMenu();
					UE_LOG(LogTemp, Log, TEXT("[PlayerController] 초기 UI (EntranceMenu) 표시 - 기타 모드"));
				}
			}
			// MainScreenWidget이 없으면 기존 방식대로 위젯 표시
			else
			{
				// ListenServer 모드: 세션이 있으면 LobbyMenu, 없으면 MainMenu
				if (NetMode == NM_ListenServer)
				{
					// 세션이 있거나 방 생성 후 재로드 상태 - LobbyMenu 표시
					if (bHasActiveSession || (bDidCreateRoomThenTravel && bHasPlayers))
					{
						if (LobbyMenuWidgetClass)
						{
							ShowLobbyMenu();
							UE_LOG(LogTemp, Log, TEXT("[PlayerController] 초기 UI (LobbyMenu) 표시 - ListenServer 모드 (세션 있음)"));
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("[PlayerController] LobbyMenuWidgetClass가 설정되지 않았습니다."));
						}
					}
					else
					{
						// 세션이 없음 - MainMenu 표시 (처음 실행 시)
						if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(W->GetGameInstance()))
						{
							BRGI->SetDidCreateRoomThenTravel(false);
						}
						if (EntranceMenuWidgetClass)
						{
							ShowEntranceMenu();
							UE_LOG(LogTemp, Log, TEXT("[PlayerController] 초기 UI (EntranceMenu) 표시 - ListenServer 모드 (세션 없음)"));
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("[PlayerController] EntranceMenuWidgetClass가 설정되지 않았습니다."));
						}
					}
				}
				// Client 모드이면 LobbyMenu 표시
				else if (NetMode == NM_Client)
				{
					if (LobbyMenuWidgetClass)
					{
						ShowLobbyMenu();
						UE_LOG(LogTemp, Log, TEXT("[PlayerController] 초기 UI (LobbyMenu) 표시 - Client 모드"));
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[PlayerController] LobbyMenuWidgetClass가 설정되지 않았습니다."));
					}
				}
				// Standalone 모드이면 EntranceMenu 표시 (단, 방 생성 후 재로드인 경우는 예외)
				else
				{
					// 방 생성 후 ServerTravel로 인한 재로드인 경우 LobbyMenu 표시
					// 세션이 없어도 플레이어가 있고 방 생성 플래그가 있으면 LobbyMenu 표시
					// 세션 체크는 제거 - ServerTravel 직후 세션이 아직 초기화되지 않을 수 있음
					bool bShouldShowLobby = bDidCreateRoomThenTravel && bHasPlayers;
					
					UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Standalone 모드 UI 결정 (MainScreenWidget 없음): bShouldShowLobby=%s (bDidCreateRoomThenTravel=%s, bHasPlayers=%s)"),
						bShouldShowLobby ? TEXT("Yes") : TEXT("No"),
						bDidCreateRoomThenTravel ? TEXT("Yes") : TEXT("No"),
						bHasPlayers ? TEXT("Yes") : TEXT("No"));
					
					if (bShouldShowLobby)
					{
						// 방 생성 후 재로드 상태 - LobbyMenu 표시
						if (LobbyMenuWidgetClass)
						{
							ShowLobbyMenu();
							UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 초기 UI (LobbyMenu) 표시 - Standalone 모드이지만 방 생성 후 재로드 상태"));
						}
					}
					else
					{
						// 일반 Standalone 모드 - EntranceMenu 표시
						if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(W->GetGameInstance()))
						{
							BRGI->SetDidCreateRoomThenTravel(false);
						}
						if (EntranceMenuWidgetClass)
						{
							ShowEntranceMenu();
							UE_LOG(LogTemp, Log, TEXT("[PlayerController] 초기 UI (EntranceMenu) 표시 - Standalone 모드"));
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("[PlayerController] EntranceMenuWidgetClass가 설정되지 않았습니다. 블루프린트에서 설정해주세요."));
						}
					}
				}
			}
		}, 0.45f, false);
	}
}

// 서버: 내가 누군가에게 빙의했을 때
void ABRPlayerController::OnPossess(APawn* aPawn)
{
	Super::OnPossess(aPawn);

	// Seamless Travel 후 게임 맵에서 GameMode BeginPlay가 호출되지 않을 수 있음 → Possess 시점에 랜덤 팀 적용 예약 (폴백)
	if (HasAuthority() && aPawn)
	{
		if (UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance()))
		{
			if (GI->GetPendingApplyRandomTeamRoles())
				{
					ABRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ABRGameMode>() : nullptr;
					if (GM)
					{
						FTimerHandle H;
						GetWorld()->GetTimerManager().SetTimer(H, GM, &ABRGameMode::ApplyRoleChangesForRandomTeams, 1.5f, false);
						UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 적용] OnPossess 폴백 - 1.5초 후 상체/하체 Pawn 적용 예정"));
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 적용] 게임 맵 GameMode가 ABRGameMode가 아님 - 상체/하체 적용이 되지 않을 수 있습니다."));
					}
				}
		}
	}

	if (OnPawnChanged.IsBound())
	{
		OnPawnChanged.Broadcast(aPawn);
	}
}

// 클라이언트: 네트워크를 통해 내 폰 정보가 갱신되었을 때
void ABRPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();

	// 상체 Pawn으로 복제 수신 시, BeginPlay가 Controller 설정 전에 호출될 수 있어
	// 마우스(Look) 입력이 등록되지 않는 경우 방지: 여기서 입력/뷰 강제 설정
	if (APawn* MyPawn = GetPawn())
	{
		if (IsLocalController() && MyPawn->IsA<AUpperBodyPawn>())
		{
			SetupRoleInput(false); // 상체 IMC 등록
			SetViewTarget(MyPawn);
			SetIgnoreMoveInput(true);
		}
	}

	if (OnPawnChanged.IsBound())
	{
		OnPawnChanged.Broadcast(GetPawn());
	}
}

void ABRPlayerController::ClearUIForShutdown()
{
	// PIE 종료 시 월드 참조 잔류 방지: GEngine/GameSession 델리게이트를 먼저 끊는다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeginPlayUITimerHandle);
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
			{
				GameSession->OnCreateSessionComplete.RemoveAll(this);
			}
		}
	}
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
	}

	if (MainScreenWidget && IsValid(MainScreenWidget))
	{
		MainScreenWidget->RemoveFromParent();
		MainScreenWidget = nullptr;
	}
	if (CurrentMenuWidget && IsValid(CurrentMenuWidget))
	{
		CurrentMenuWidget->RemoveFromParent();
		CurrentMenuWidget = nullptr;
	}
}

void ABRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// PIE/서버 종료 시 위젯을 먼저 정리 — WBP_MainScreen·WBP_EntranceMenu 등이
	// GetBRPlayerController → SetMainScreenWidget/CreateRoomWithPlayerName 호출 시
	// PC가 이미 None이 되어 "Accessed None" 크래시가 나는 것을 줄이기 위함
	ClearUIForShutdown();

	// BeginPlay UI 타이머 해제 (open ?listen 맵 전환 시 파괴 후 콜백 크래시 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeginPlayUITimerHandle);
	}

	// 네트워크 연결 실패 델리게이트 언바인딩
	if (UEngine* Engine = GEngine)
	{
		Engine->OnNetworkFailure().RemoveAll(this);
	}

	// GameSession 이벤트 언바인딩
	if (UWorld* World = GetWorld())
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
			{
				GameSession->OnCreateSessionComplete.RemoveAll(this);
			}
		}
	}
	
	Super::EndPlay(EndPlayReason);
}

void ABRPlayerController::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// 현재 World가 이 PlayerController의 World인지 확인
	if (World != GetWorld())
	{
		return;
	}
	
	FString FailureTypeString;
	switch (FailureType)
	{
	case ENetworkFailure::NetDriverAlreadyExists:
		FailureTypeString = TEXT("NetDriverAlreadyExists");
		break;
	case ENetworkFailure::NetDriverCreateFailure:
		FailureTypeString = TEXT("NetDriverCreateFailure");
		break;
	case ENetworkFailure::NetDriverListenFailure:
		FailureTypeString = TEXT("NetDriverListenFailure");
		break;
	case ENetworkFailure::ConnectionLost:
		FailureTypeString = TEXT("ConnectionLost");
		break;
	case ENetworkFailure::ConnectionTimeout:
		FailureTypeString = TEXT("ConnectionTimeout");
		break;
	case ENetworkFailure::FailureReceived:
		FailureTypeString = TEXT("FailureReceived");
		break;
	case ENetworkFailure::OutdatedClient:
		FailureTypeString = TEXT("OutdatedClient");
		break;
	case ENetworkFailure::OutdatedServer:
		FailureTypeString = TEXT("OutdatedServer");
		break;
	case ENetworkFailure::PendingConnectionFailure:
		FailureTypeString = TEXT("PendingConnectionFailure");
		break;
	case ENetworkFailure::NetGuidMismatch:
		FailureTypeString = TEXT("NetGuidMismatch");
		break;
	case ENetworkFailure::NetChecksumMismatch:
		FailureTypeString = TEXT("NetChecksumMismatch");
		break;
	default:
		FailureTypeString = TEXT("Unknown");
		break;
	}
	
	UE_LOG(LogTemp, Error, TEXT("[방 참가] 네트워크 연결 실패 감지!"));
	UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패 유형: %s"), *FailureTypeString);
	UE_LOG(LogTemp, Error, TEXT("[방 참가] 오류 메시지: %s"), *ErrorString);
	
	if (NetDriver)
	{
		FString ServerAddress = NetDriver->ServerConnection ? 
			NetDriver->ServerConnection->LowLevelGetRemoteAddress(true) : TEXT("Unknown");
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 서버 주소: %s"), *ServerAddress);
	}
	
	// 일반적인 원인 안내
	if (FailureType == ENetworkFailure::ConnectionTimeout || FailureType == ENetworkFailure::PendingConnectionFailure)
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 가능한 원인:"));
		UE_LOG(LogTemp, Error, TEXT("  1. 서버가 실행 중이지 않음"));
		UE_LOG(LogTemp, Error, TEXT("  2. 방화벽이 포트를 차단함 (포트 7777 확인)"));
		UE_LOG(LogTemp, Error, TEXT("  3. 서버가 다른 맵을 로드하지 않음"));
		UE_LOG(LogTemp, Error, TEXT("  4. 네트워크 연결 문제"));
	}
}

void ABRPlayerController::CreateRoom(const FString& RoomName)
{
	UE_LOG(LogTemp, Error, TEXT("[방 생성] CreateRoom 호출됨: %s"), *RoomName);
	
	UWorld* World = GetWorld();
	if (World)
	{
		ENetMode NetMode = World->GetNetMode();
		UE_LOG(LogTemp, Error, TEXT("[방 생성] 현재 NetMode: %s"), 
			NetMode == NM_Standalone ? TEXT("Standalone") :
			NetMode == NM_ListenServer ? TEXT("ListenServer") :
			NetMode == NM_Client ? TEXT("Client") :
			NetMode == NM_DedicatedServer ? TEXT("DedicatedServer") : TEXT("Unknown"));
	}
	
	// 방 생성 요청 시 플래그 설정 (ServerTravel 후 재로드 시 감지용)
	if (World)
	{
		if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(World->GetGameInstance()))
		{
			BRGI->SetDidCreateRoomThenTravel(true);
			UE_LOG(LogTemp, Error, TEXT("[방 생성] SetDidCreateRoomThenTravel 플래그 설정"));
		}
	}
	
	if (HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 생성] 서버 권한 있음 - 직접 실행"));
		// 서버에서 직접 실행
		if (World)
		{
			if (AGameModeBase* GameMode = World->GetAuthGameMode())
			{
				UE_LOG(LogTemp, Error, TEXT("[방 생성] GameMode 발견: %s"), *GameMode->GetClass()->GetName());
				if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
				{
					UE_LOG(LogTemp, Error, TEXT("[방 생성] GameSession 발견! 세션 생성 요청 중..."));
					GameSession->CreateRoomSession(RoomName);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[방 생성] ❌ GameSession을 찾을 수 없습니다."));
					if (GameMode->GameSession)
					{
						UE_LOG(LogTemp, Error, TEXT("[방 생성] GameMode->GameSession 타입: %s (ABRGameSession이 아님)"), 
							*GameMode->GameSession->GetClass()->GetName());
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("[방 생성] GameMode->GameSession이 NULL입니다."));
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[방 생성] ❌ GameMode를 찾을 수 없습니다."));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[방 생성] 클라이언트 권한 - 서버로 RPC 전송..."));
		// 클라이언트에서는 서버로 RPC 전송
		ServerCreateRoom(RoomName);
	}
}

void ABRPlayerController::CreateRoomWithPlayerName(const FString& RoomName, const FString& PlayerName)
{
	UE_LOG(LogTemp, Error, TEXT("========================================"));
	UE_LOG(LogTemp, Error, TEXT("[방 생성] CreateRoomWithPlayerName 호출됨!"));
	UE_LOG(LogTemp, Error, TEXT("[방 생성] 방 이름: %s, 플레이어 이름: %s"), *RoomName, *PlayerName);
	UE_LOG(LogTemp, Error, TEXT("========================================"));
	
	// GameInstance에 이름 저장 (ServerTravel 후 PostLogin에서 적용 → UserInfo/로비 UI에 정상 반영)
	if (UWorld* World = GetWorld())
	{
		if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(World->GetGameInstance()))
		{
			BRGI->SetPlayerName(PlayerName);
		}
	}
	// 먼저 플레이어 이름 설정
	SetPlayerName(PlayerName);
	// 그 다음 방 생성
	CreateRoom(RoomName);
}

void ABRPlayerController::FindRooms()
{
	UE_LOG(LogTemp, Log, TEXT("[방 찾기] 명령 실행"));
	
	// 화면에 디버그 메시지 표시
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[PlayerController] 방 찾기 요청..."));
	}
	
	if (HasAuthority())
	{
		// 서버에서 직접 실행
		if (UWorld* World = GetWorld())
		{
			if (AGameModeBase* GameMode = World->GetAuthGameMode())
			{
				if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
				{
					UE_LOG(LogTemp, Log, TEXT("[방 찾기] 세션 검색 시작..."));
					GameSession->FindSessions();
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[방 찾기] 실패: GameSession을 찾을 수 없습니다."));
					
					// 화면에 디버그 메시지 표시
					if (GEngine)
					{
						GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 찾기] 실패: GameSession을 찾을 수 없습니다!"));
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[방 찾기] 실패: GameMode를 찾을 수 없습니다."));
				
				// 화면에 디버그 메시지 표시
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 찾기] 실패: GameMode를 찾을 수 없습니다!"));
				}
			}
		}
	}
	else
	{
		// 클라이언트에서는 서버로 RPC 전송
		UE_LOG(LogTemp, Log, TEXT("[방 찾기] 클라이언트에서 서버로 요청 전송..."));
		ServerFindRooms();
	}
}

void ABRPlayerController::JoinRoom(int32 SessionIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[방 참가] 명령 실행: 세션 인덱스=%d"), SessionIndex);
	
	// 화면에 디버그 메시지 표시
	if (GEngine)
	{
		FString Message = FString::Printf(TEXT("[방 참가] 요청: 세션 인덱스 %d"), SessionIndex);
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Message);
	}
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: World를 찾을 수 없습니다."));
		return;
	}

	// 네트워크 모드 확인
	ENetMode NetMode = World->GetNetMode();
	
	// ListenServer나 DedicatedServer는 JoinRoom을 실행할 수 없음
	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 참가] 호스트는 이미 서버이므로 다른 방에 참가할 수 없습니다."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("[방 참가] 호스트는 다른 방에 참가할 수 없습니다!"));
		}
		return;
	}

	// [수정] 클라이언트에서 직접 로컬 GameSession을 통해 참가 시도
	// 기존에는 서버 RPC를 호출했으나, 세션 참가는 클라이언트 시스템에서 이루어져야 함
	if (IsLocalController())
	{
		UE_LOG(LogTemp, Log, TEXT("[방 참가] 로컬 컨트롤러: 직접 세션 참가 시도..."));
		
		ABRGameSession* BRGameSession = nullptr;
		
		// 1. GameMode에서 GameSession 가져오기 (Standalone 모드용)
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			BRGameSession = Cast<ABRGameSession>(GameMode->GameSession);
		}
		
		// 2. GameMode에 없으면 (NM_Client 모드 등), 직접 GameSession 찾기
		if (!BRGameSession)
		{
			for (TActorIterator<ABRGameSession> It(World); It; ++It)
			{
				BRGameSession = *It;
				break;
			}
		}

		if (BRGameSession)
		{
			UE_LOG(LogTemp, Log, TEXT("[방 참가] BRGameSession 발견, JoinSessionByIndex(%d) 호출"), SessionIndex);
			BRGameSession->JoinSessionByIndex(SessionIndex);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: ABRGameSession을 찾을 수 없습니다."));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 참가] 실패: 세션 관리자를 찾을 수 없습니다!"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 참가] 로컬 컨트롤러가 아닙니다."));
	}
}

void ABRPlayerController::JoinRoomWithPlayerName(int32 SessionIndex, const FString& PlayerName)
{
	// Standalone 모드에서도 확인 가능하도록 화면에 큰 메시지 표시
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] JoinRoomWithPlayerName 호출됨"));
	UE_LOG(LogTemp, Warning, TEXT("세션 인덱스: %d, 플레이어 이름: %s"), SessionIndex, *PlayerName);
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	
	// 화면에 디버그 메시지 표시 (Standalone 모드에서도 보이도록)
	if (GEngine)
	{
		FString Message = FString::Printf(TEXT(">>> [PlayerController] JoinRoomWithPlayerName: 세션 %d, 이름 %s <<<"), SessionIndex, *PlayerName);
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, Message);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
	}
	
	// GameInstance에 이름 저장 (참가 후 PostLogin에서 적용 → UserInfo/로비 UI에 정상 반영)
	if (UWorld* World = GetWorld())
	{
		if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(World->GetGameInstance()))
		{
			BRGI->SetPlayerName(PlayerName);
		}
	}
	// 먼저 플레이어 이름 설정
	SetPlayerName(PlayerName);
	// 그 다음 방 참가
	JoinRoom(SessionIndex);
}

void ABRPlayerController::ToggleReady()
{
	UE_LOG(LogTemp, Log, TEXT("[준비 상태] 토글 명령 실행"));
	if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
	{
		bool bWasReady = BRPS->bIsReady;
		if (HasAuthority())
		{
			BRPS->ToggleReady();
			UE_LOG(LogTemp, Log, TEXT("[준비 상태] 변경: %s -> %s"), 
				bWasReady ? TEXT("준비 완료") : TEXT("대기 중"),
				BRPS->bIsReady ? TEXT("준비 완료") : TEXT("대기 중"));
		}
		else
		{
			// 클라이언트에서 서버로 요청
			UE_LOG(LogTemp, Log, TEXT("[준비 상태] 서버에 요청 전송..."));
			ServerToggleReady();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[준비 상태] 실패: PlayerState를 찾을 수 없습니다."));
	}
}

void ABRPlayerController::RandomTeams()
{
	UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 명령 실행"));
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[랜덤 팀 배정] 실패: World를 찾을 수 없습니다."));
		return;
	}
	
	// 서버(호스트)인 경우 자동으로 사용 가능
	// 클라이언트인 경우 방장인지 확인
	bool bCanUse = false;
	if (HasAuthority())
	{
		// 서버인 경우 자동으로 사용 가능
		bCanUse = true;
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 서버 권한 확인 완료, 팀 배정 시작..."));
	}
	else
	{
		// 클라이언트인 경우 방장인지 확인
		if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
		{
			if (BRPS->bIsHost)
			{
				bCanUse = true;
				UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 방장 권한 확인 완료, 팀 배정 시작..."));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 배정] 실패: 방장만 사용할 수 있습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[랜덤 팀 배정] 실패: PlayerState를 찾을 수 없습니다."));
		}
	}
	
	if (bCanUse)
	{
		RequestRandomTeams();
	}
}

void ABRPlayerController::ChangeTeam(int32 PlayerIndex, int32 TeamNumber)
{
	UE_LOG(LogTemp, Log, TEXT("[팀 변경] 명령 실행: PlayerIndex=%d, TeamNumber=%d"), PlayerIndex, TeamNumber);
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[팀 변경] 실패: World를 찾을 수 없습니다."));
		return;
	}
	
	// 서버(호스트)인 경우 자동으로 사용 가능
	// 클라이언트인 경우 방장인지 확인
	bool bCanUse = false;
	if (HasAuthority())
	{
		// 서버인 경우 자동으로 사용 가능
		bCanUse = true;
		UE_LOG(LogTemp, Log, TEXT("[팀 변경] 서버 권한 확인 완료, 팀 변경 요청 중..."));
	}
	else
	{
		// 클라이언트인 경우 방장인지 확인
		if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
		{
			if (BRPS->bIsHost)
			{
				bCanUse = true;
				UE_LOG(LogTemp, Log, TEXT("[팀 변경] 방장 권한 확인 완료, 팀 변경 요청 중..."));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[팀 변경] 실패: 방장만 사용할 수 있습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[팀 변경] 실패: PlayerState를 찾을 수 없습니다."));
		}
	}
	
	if (bCanUse)
	{
		RequestChangePlayerTeam(PlayerIndex, TeamNumber);
	}
}

void ABRPlayerController::StartGame()
{
	UE_LOG(LogTemp, Log, TEXT("[게임 시작] 명령 실행"));
	
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[게임 시작] 실패: World를 찾을 수 없습니다."));
		return;
	}
	
	// 서버(호스트)인 경우 자동으로 게임 시작 가능
	// 클라이언트인 경우 방장인지 확인
	bool bCanStart = false;
	if (HasAuthority())
	{
		// 서버인 경우 자동으로 시작 가능
		bCanStart = true;
		UE_LOG(LogTemp, Log, TEXT("[게임 시작] 서버 권한 확인 완료, 게임 시작 요청 중..."));
	}
	else
	{
		// 클라이언트인 경우 방장인지 확인
		if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
		{
			if (BRPS->bIsHost)
			{
				bCanStart = true;
				UE_LOG(LogTemp, Log, TEXT("[게임 시작] 방장 권한 확인 완료, 게임 시작 요청 중..."));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[게임 시작] 실패: 방장만 사용할 수 있습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[게임 시작] 실패: PlayerState를 찾을 수 없습니다."));
		}
	}
	
	if (bCanStart)
	{
		RequestStartGame();
	}
}

// 서버 RPC 함수들
void ABRPlayerController::ServerCreateRoom_Implementation(const FString& RoomName)
{
	CreateRoom(RoomName);
}

void ABRPlayerController::ServerFindRooms_Implementation()
{
	FindRooms();
}

void ABRPlayerController::ServerJoinRoom_Implementation(int32 SessionIndex)
{
	JoinRoom(SessionIndex);
}

void ABRPlayerController::ServerToggleReady_Implementation()
{
	ToggleReady();
}

void ABRPlayerController::ServerRequestRandomTeams_Implementation()
{
	// 서버에서 직접 팀 배정 실행 (역할/팀 번호만 설정, 상체 스폰·빙의는 게임 맵 로드 후 적용)
	if (ABRGameState* BRGameState = GetWorld()->GetGameState<ABRGameState>())
	{
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 서버에서 직접 실행: 총 %d명의 플레이어"), BRGameState->PlayerArray.Num());
		BRGameState->AssignRandomTeams();
		UBRGameInstance* GI = GetWorld() ? Cast<UBRGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
		if (GI)
		{
			GI->SavePendingRolesForTravel(BRGameState);
			GI->SetPendingApplyRandomTeamRoles(true);
			UE_LOG(LogTemp, Warning, TEXT("[랜덤 팀 배정] 역할 저장 완료, 게임 맵 이동 시 상체/하체 Pawn 적용 예약"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[랜덤 팀 배정] GameInstance 없음 - 역할 저장 스킵, 상체/하체 복원 실패 가능"));
		}
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 완료"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[랜덤 팀 배정] 실패: GameState를 찾을 수 없습니다."));
	}
}

void ABRPlayerController::ServerRequestChangePlayerTeam_Implementation(int32 PlayerIndex, int32 NewTeamNumber)
{
	// 서버에서 직접 팀 변경 실행
	if (ABRGameState* BRGameState = GetWorld()->GetGameState<ABRGameState>())
	{
		if (PlayerIndex >= 0 && PlayerIndex < BRGameState->PlayerArray.Num())
		{
			if (ABRPlayerState* TargetPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[PlayerIndex]))
			{
				FString PlayerName = TargetPS->GetPlayerName();
				if (PlayerName.IsEmpty())
				{
					PlayerName = FString::Printf(TEXT("Player %d"), PlayerIndex + 1);
				}
				TargetPS->SetTeamNumber(NewTeamNumber);
				UE_LOG(LogTemp, Log, TEXT("[팀 변경] 서버에서 직접 실행: %s -> 팀 %d"), *PlayerName, NewTeamNumber);
				BRGameState->OnTeamChanged.Broadcast();
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[팀 변경] 실패: PlayerState를 찾을 수 없습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[팀 변경] 실패: 잘못된 PlayerIndex (%d)"), PlayerIndex);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[팀 변경] 실패: GameState를 찾을 수 없습니다."));
	}
}

void ABRPlayerController::ServerRequestStartGame_Implementation()
{
	StartGame();
}

void ABRPlayerController::ServerSetPlayerName_Implementation(const FString& NewPlayerName)
{
	UE_LOG(LogTemp, Warning, TEXT("[로비이름] ServerSetPlayerName 수신 | NewPlayerName='%s' | HasAuthority=%d"), *NewPlayerName, HasAuthority() ? 1 : 0);
	if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
	{
		FString OldName = BRPS->GetPlayerName();
		FString OldUID = BRPS->UserUID;
		BRPS->SetPlayerNameString(NewPlayerName);
		UE_LOG(LogTemp, Warning, TEXT("[로비이름] ServerSetPlayerName 적용 | 이전 PlayerName='%s' UserUID='%s' → 새 PlayerName='%s'"), *OldName, *OldUID, *BRPS->GetPlayerName());
		if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
		{
			GS->UpdatePlayerList(); // 복제 목록 갱신 → 모든 클라이언트에 새 이름 반영
		}
	}
}

void ABRPlayerController::ServerSetTeamNumber_Implementation(int32 NewTeamNumber)
{
	if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
	{
		BRPS->SetTeamNumber(NewTeamNumber);
		UE_LOG(LogTemp, Log, TEXT("[팀 번호 설정] 서버에서 설정: 팀 %d"), NewTeamNumber);
	}
}

void ABRPlayerController::ServerSetPlayerRole_Implementation(bool bLowerBody)
{
	if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
	{
		// 현재는 연결된 플레이어 인덱스를 -1로 설정 (나중에 연결 로직 추가 가능)
		BRPS->SetPlayerRole(bLowerBody, -1);
		FString RoleName = bLowerBody ? TEXT("하체") : TEXT("상체");
		UE_LOG(LogTemp, Log, TEXT("[플레이어 역할 설정] 서버에서 설정: %s"), *RoleName);
	}
}

void ABRPlayerController::RequestAssignToLobbyTeam(int32 TeamIndex, int32 SlotIndex)
{
	ABRGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABRGameState>() : nullptr;
	if (!GS) return;
	if (HasAuthority())
	{
		APlayerState* PS = GetPlayerState<APlayerState>();
		int32 PlayerIndex = PS ? GS->PlayerArray.Find(PS) : INDEX_NONE;
		if (PlayerIndex != INDEX_NONE && GS->AssignPlayerToLobbyTeam(PlayerIndex, TeamIndex, SlotIndex))
		{
			UE_LOG(LogTemp, Log, TEXT("[로비] 플레이어 %d -> 팀 %d 슬롯 %d 배치"), PlayerIndex, TeamIndex + 1, SlotIndex + 1);
		}
		return;
	}
	ServerRequestAssignToLobbyTeam(TeamIndex, SlotIndex);
}

void ABRPlayerController::RequestMoveToLobbyEntry(int32 TeamIndex, int32 SlotIndex)
{
	ABRGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABRGameState>() : nullptr;
	if (!GS) return;
	if (HasAuthority())
	{
		if (GS->MovePlayerToLobbyEntry(TeamIndex, SlotIndex))
		{
			UE_LOG(LogTemp, Log, TEXT("[로비] 팀 %d 슬롯 %d -> Entry 이동"), TeamIndex + 1, SlotIndex + 1);
		}
		return;
	}
	ServerRequestMoveToLobbyEntry(TeamIndex, SlotIndex);
}

void ABRPlayerController::ServerRequestAssignToLobbyTeam_Implementation(int32 TeamIndex, int32 SlotIndex)
{
	ABRGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABRGameState>() : nullptr;
	if (!GS || !HasAuthority()) return;
	APlayerState* PS = GetPlayerState<APlayerState>();
	int32 PlayerIndex = PS ? GS->PlayerArray.Find(PS) : INDEX_NONE;
	if (PlayerIndex != INDEX_NONE && GS->AssignPlayerToLobbyTeam(PlayerIndex, TeamIndex, SlotIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("[로비] 서버: 플레이어 %d -> 팀 %d 슬롯 %d 배치"), PlayerIndex, TeamIndex + 1, SlotIndex + 1);
	}
}

void ABRPlayerController::ServerRequestMoveToLobbyEntry_Implementation(int32 TeamIndex, int32 SlotIndex)
{
	ABRGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABRGameState>() : nullptr;
	if (!GS || !HasAuthority()) return;
	if (GS->MovePlayerToLobbyEntry(TeamIndex, SlotIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("[로비] 서버: 팀 %d 슬롯 %d -> Entry 이동"), TeamIndex + 1, SlotIndex + 1);
	}
}

void ABRPlayerController::ClientNotifyGameStarting_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("[게임 시작] 클라이언트: 게임 시작 알림 수신 - 맵 이동 대기 중..."));
	
	// 클라이언트에서 입력을 일시적으로 중지 (선택사항)
	// 실제로는 ServerTravel이 자동으로 클라이언트를 따라오므로 특별한 처리가 필요하지 않을 수 있습니다
	// 하지만 로그를 남겨서 알림이 도착했는지 확인할 수 있습니다

	if (UWorld* World = GetWorld())
	{
		if (UBRGameInstance* GI = Cast<UBRGameInstance>(World->GetGameInstance()))
		{
			GI->ClearCachedRoomTitle();
		}
	}
}

void ABRPlayerController::ClientReceiveRoomTitle_Implementation(const FString& RoomTitle)
{
	UE_LOG(LogTemp, Log, TEXT("[방 제목] 클라이언트 수신: %s"), *RoomTitle);

	if (UWorld* World = GetWorld())
	{
		if (UBRGameInstance* GI = Cast<UBRGameInstance>(World->GetGameInstance()))
		{
			GI->SetCachedRoomTitle(RoomTitle);
			GI->OnRoomTitleReceived.Broadcast();
		}
	}
}

void ABRPlayerController::RequestRandomTeams()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[랜덤 팀 배정] 실패: World를 찾을 수 없습니다."));
		return;
	}
	
	// 서버에서 직접 실행 (역할/팀 번호만 설정, 상체 스폰·빙의는 게임 맵 로드 후 적용)
	if (HasAuthority())
	{
		if (ABRGameState* BRGameState = World->GetGameState<ABRGameState>())
		{
			UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 서버에서 직접 실행: 총 %d명의 플레이어"), BRGameState->PlayerArray.Num());
			BRGameState->AssignRandomTeams();
			if (UBRGameInstance* GI = Cast<UBRGameInstance>(World->GetGameInstance()))
			{
				GI->SetPendingApplyRandomTeamRoles(true);
				UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 게임 맵 이동 시 상체/하체 Pawn 적용 예약됨"));
			}
			UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 완료"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[랜덤 팀 배정] 실패: GameState를 찾을 수 없습니다."));
		}
	}
	else
	{
		// 클라이언트에서 서버로 요청
		UE_LOG(LogTemp, Log, TEXT("[랜덤 팀 배정] 클라이언트에서 서버로 요청 전송..."));
		ServerRequestRandomTeams();
	}
}

void ABRPlayerController::RequestChangePlayerTeam(int32 PlayerIndex, int32 NewTeamNumber)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[팀 변경] 실패: World를 찾을 수 없습니다."));
		return;
	}
	
	// 서버에서 직접 실행
	if (HasAuthority())
	{
		if (ABRGameState* BRGameState = World->GetGameState<ABRGameState>())
		{
			if (PlayerIndex >= 0 && PlayerIndex < BRGameState->PlayerArray.Num())
			{
				if (ABRPlayerState* TargetPS = Cast<ABRPlayerState>(BRGameState->PlayerArray[PlayerIndex]))
				{
					FString PlayerName = TargetPS->GetPlayerName();
					if (PlayerName.IsEmpty())
					{
						PlayerName = FString::Printf(TEXT("Player %d"), PlayerIndex + 1);
					}
					int32 OldTeam = TargetPS->TeamNumber;
					TargetPS->SetTeamNumber(NewTeamNumber);
					UE_LOG(LogTemp, Log, TEXT("[팀 변경] 서버에서 직접 실행: %s의 팀이 %d에서 %d로 변경되었습니다."), 
						*PlayerName, OldTeam, NewTeamNumber);
					BRGameState->OnTeamChanged.Broadcast();
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[팀 변경] 실패: PlayerState 캐스팅 실패"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[팀 변경] 실패: 잘못된 PlayerIndex (%d), 현재 플레이어 수: %d"), 
					PlayerIndex, BRGameState->PlayerArray.Num());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[팀 변경] 실패: GameState를 찾을 수 없습니다."));
		}
	}
	else
	{
		// 클라이언트에서 서버로 요청
		UE_LOG(LogTemp, Log, TEXT("[팀 변경] 클라이언트에서 서버로 요청 전송..."));
		ServerRequestChangePlayerTeam(PlayerIndex, NewTeamNumber);
	}
}

void ABRPlayerController::SetPlayerName(const FString& NewPlayerName)
{
	UE_LOG(LogTemp, Warning, TEXT("[로비이름] SetPlayerName 호출 | NewPlayerName='%s' | HasAuthority=%d (0=클라이언트→서버 RPC 전송)"), *NewPlayerName, HasAuthority() ? 1 : 0);
	if (HasAuthority())
	{
		if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
		{
			BRPS->SetPlayerNameString(NewPlayerName);
		}
	}
	else
	{
		ServerSetPlayerName(NewPlayerName);
	}
}

void ABRPlayerController::SetMyTeamNumber(int32 NewTeamNumber)
{
	if (HasAuthority())
	{
		if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
		{
			BRPS->SetTeamNumber(NewTeamNumber);
		}
	}
	else
	{
		ServerSetTeamNumber(NewTeamNumber);
	}
}

void ABRPlayerController::SetMyPlayerRole(bool bLowerBody)
{
	if (HasAuthority())
	{
		if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
		{
			BRPS->SetPlayerRole(bLowerBody, -1);
		}
	}
	else
	{
		ServerSetPlayerRole(bLowerBody);
	}
}

void ABRPlayerController::RequestStartGame()
{
	if (ABRPlayerState* BRPS = GetPlayerState<ABRPlayerState>())
	{
		if (BRPS->bIsHost && HasAuthority())
		{
			if (ABRGameMode* BRGameMode = GetWorld()->GetAuthGameMode<ABRGameMode>())
			{
				if (ABRGameState* BRGameState = GetWorld()->GetGameState<ABRGameState>())
				{
					UE_LOG(LogTemp, Log, TEXT("[게임 시작] 조건 확인: 플레이어 수=%d/%d, 모든 플레이어 준비=%s"), 
						BRGameState->PlayerCount, BRGameState->MaxPlayers,
						BRGameState->AreAllPlayersReady() ? TEXT("예") : TEXT("아니오"));
				}
				BRGameMode->StartGame();
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[게임 시작] 실패: GameMode를 찾을 수 없습니다."));
			}
		}
		else
		{
			// 클라이언트에서 서버로 요청
			ServerRequestStartGame();
		}
	}
}

void ABRPlayerController::ShowRoomInfo()
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	// 네트워크 상태 확인
	ENetMode NetMode = World->GetNetMode();
	FString NetModeString;
	switch (NetMode)
	{
	case NM_Standalone:
		NetModeString = TEXT("Standalone (로컬)");
		break;
	case NM_DedicatedServer:
		NetModeString = TEXT("Dedicated Server");
		break;
	case NM_ListenServer:
		NetModeString = TEXT("Listen Server (호스트)");
		break;
	case NM_Client:
		NetModeString = TEXT("Client (클라이언트)");
		break;
	default:
		NetModeString = TEXT("Unknown");
		break;
	}

	ABRGameState* BRGameState = World->GetGameState<ABRGameState>();
	if (!BRGameState)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameState를 찾을 수 없습니다."));
		return;
	}

	ABRPlayerState* LocalPS = GetPlayerState<ABRPlayerState>();
	if (!LocalPS)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerState를 찾을 수 없습니다."));
		return;
	}

	// 연결 상태 확인
	bool bIsConnected = (NetMode == NM_Client || NetMode == NM_ListenServer);
	FString ConnectionStatus = bIsConnected ? TEXT("연결됨") : TEXT("연결 안 됨 (Standalone)");
	
	// 서버 연결 확인 (클라이언트인 경우)
	bool bIsActuallyConnected = false;
	if (NetMode == NM_Client)
	{
		if (UNetConnection* Connection = GetNetConnection())
		{
			// Connection이 존재하면 연결된 것으로 간주
			bIsActuallyConnected = true;
			FString RemoteAddr = Connection->LowLevelGetRemoteAddress(true);
			UE_LOG(LogTemp, Log, TEXT("서버 주소: %s"), *RemoteAddr);
		}
	}
	
	// 방 정보 출력
	UE_LOG(LogTemp, Log, TEXT("=== 방 정보 ==="));
	UE_LOG(LogTemp, Log, TEXT("네트워크 모드: %s"), *NetModeString);
	UE_LOG(LogTemp, Log, TEXT("연결 상태: %s"), *ConnectionStatus);
	if (NetMode == NM_Client)
	{
		UE_LOG(LogTemp, Log, TEXT("실제 연결 상태: %s"), bIsActuallyConnected ? TEXT("연결됨") : TEXT("연결 안 됨"));
	}
	UE_LOG(LogTemp, Log, TEXT("서버 권한: %s"), HasAuthority() ? TEXT("예 (서버)") : TEXT("아니오 (클라이언트)"));
	
	// PlayerArray와 PlayerCount 비교
	int32 ActualPlayerCount = BRGameState->PlayerArray.Num();
	UE_LOG(LogTemp, Log, TEXT("플레이어 수 (PlayerCount): %d / %d (최소: %d)"), BRGameState->PlayerCount, BRGameState->MaxPlayers, BRGameState->MinPlayers);
	UE_LOG(LogTemp, Log, TEXT("플레이어 수 (PlayerArray): %d명"), ActualPlayerCount);
	
	if (ActualPlayerCount != BRGameState->PlayerCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("경고: PlayerCount(%d)와 PlayerArray(%d)가 일치하지 않습니다!"), 
			BRGameState->PlayerCount, ActualPlayerCount);
		if (HasAuthority())
		{
			UE_LOG(LogTemp, Log, TEXT("서버에서 PlayerList 업데이트 중..."));
			BRGameState->UpdatePlayerList();
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("게임 시작 가능: %s"), BRGameState->bCanStartGame ? TEXT("예") : TEXT("아니오"));
	UE_LOG(LogTemp, Log, TEXT("내 상태 - 방장: %s, 준비: %s, 팀: %d"), 
		LocalPS->bIsHost ? TEXT("예") : TEXT("아니오"),
		LocalPS->bIsReady ? TEXT("예") : TEXT("아니오"),
		LocalPS->TeamNumber);

	// 플레이어 목록 출력
	UE_LOG(LogTemp, Log, TEXT("=== 플레이어 목록 ==="));
	if (BRGameState->PlayerArray.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("플레이어 목록이 비어있습니다!"));
		if (!HasAuthority())
		{
			UE_LOG(LogTemp, Warning, TEXT("클라이언트: 서버의 GameState가 아직 복제되지 않았을 수 있습니다."));
		}
	}
	
	for (int32 i = 0; i < BRGameState->PlayerArray.Num(); i++)
	{
		if (ABRPlayerState* PS = Cast<ABRPlayerState>(BRGameState->PlayerArray[i]))
		{
			FString PlayerName = PS->GetPlayerName();
			if (PlayerName.IsEmpty())
			{
				PlayerName = FString::Printf(TEXT("Player %d"), i + 1);
			}

			FString TeamText = PS->TeamNumber == 0 ? TEXT("팀 없음") : FString::Printf(TEXT("팀 %d"), PS->TeamNumber);
			UE_LOG(LogTemp, Log, TEXT("[%d] %s - %s, 준비: %s, 방장: %s"),
				i,
				*PlayerName,
				*TeamText,
				PS->bIsReady ? TEXT("예") : TEXT("아니오"),
				PS->bIsHost ? TEXT("예") : TEXT("아니오"));
		}
	}
	UE_LOG(LogTemp, Log, TEXT("==================="));
}

void ABRPlayerController::SetupRoleInput(bool bIsLower)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer) return;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		Subsystem->ClearAllMappings();

		UInputMappingContext* TargetContext = bIsLower ? LowerBodyContext : UpperBodyContext;
		if (TargetContext)
		{
			Subsystem->AddMappingContext(TargetContext, 0);
		}
	}

	// [추가] 하체 캐릭터라면 입력 바인딩(함수 연결)을 강제로 다시 시키기
	if (bIsLower)
	{
		if (APawn* P = GetPawn())
		{
			// 클라이언트에게 입력 시스템 재시작 명령
			ClientRestart(P);
		}
	}
}

// ========== UI 관리 함수 구현 ==========

void ABRPlayerController::ShowEntranceMenu()
{
	if (EntranceMenuWidgetClass)
	{
		ShowMenuWidget(EntranceMenuWidgetClass);
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] EntranceMenu 표시"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] EntranceMenuWidgetClass가 설정되지 않았습니다."));
	}
}

void ABRPlayerController::ShowJoinMenu()
{
	if (JoinMenuWidgetClass)
	{
		ShowMenuWidget(JoinMenuWidgetClass);
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] JoinMenu 표시"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] JoinMenuWidgetClass가 설정되지 않았습니다."));
	}
}

void ABRPlayerController::ShowLobbyMenu()
{
	// Standalone 모드에서는 LobbyMenu를 표시하지 않음
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_Standalone)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] ShowLobbyMenu: Standalone 모드에서는 LobbyMenu를 표시하지 않습니다. EntranceMenu를 표시합니다."));
		ShowEntranceMenu();
		return;
	}
	
	if (LobbyMenuWidgetClass)
	{
		ShowMenuWidget(LobbyMenuWidgetClass);
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] LobbyMenu 표시"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] LobbyMenuWidgetClass가 설정되지 않았습니다."));
	}
}

void ABRPlayerController::SetMainScreenWidget(UUserWidget* Widget)
{
	MainScreenWidget = Widget;
	UE_LOG(LogTemp, Log, TEXT("[PlayerController] MainScreenWidget 설정: %s"), Widget ? *Widget->GetName() : TEXT("None"));
	
	// MainScreenWidget이 설정되면 네트워크 모드에 따라 적절한 메뉴로 전환
	if (Widget && IsValid(Widget))
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}
		
		ENetMode NetMode = World->GetNetMode();
		
		// 클라이언트 모드: LobbyMenu로 전환
		if (NetMode == NM_Client)
		{
			SetMainScreenToLobbyMenu();
		}
		// 서버 모드(ListenServer) 또는 Standalone
		else if (NetMode == NM_ListenServer || NetMode == NM_Standalone)
		{
			// ListenServer 모드: 세션이 있으면 LobbyMenu, 없으면 MainMenu
			if (NetMode == NM_ListenServer)
			{
				// 세션 확인
				bool bHasActiveSession = false;
				if (AGameModeBase* GameMode = World->GetAuthGameMode())
				{
					if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
					{
						bHasActiveSession = GameSession->HasActiveSession();
					}
				}
				
				if (bHasActiveSession)
				{
					SetMainScreenToLobbyMenu();
				}
				else
				{
					SetMainScreenToEntranceMenu();
				}
			}
			else
			{
				// Standalone 모드: 항상 MainMenu
				SetMainScreenToEntranceMenu();
			}
		}
	}
}

void ABRPlayerController::ShowMainScreen()
{
	if (MainScreenWidget && IsValid(MainScreenWidget))
	{
		MainScreenWidget->SetVisibility(ESlateVisibility::Visible);
		
		// 현재 메뉴 위젯 제거
		if (CurrentMenuWidget)
		{
			CurrentMenuWidget->RemoveFromParent();
			CurrentMenuWidget = nullptr;
		}
		
		// 네트워크 모드에 따라 적절한 메뉴로 전환
		UWorld* World = GetWorld();
		if (World)
		{
			ENetMode NetMode = World->GetNetMode();
			if (NetMode == NM_Client)
			{
				SetMainScreenToLobbyMenu();
			}
			else if (NetMode == NM_ListenServer)
			{
				SetMainScreenToEntranceMenu();
			}
			else
			{
				SetMainScreenToEntranceMenu();
			}
		}
		
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] WBP_MainScreen 표시"));
	}
	else if (MainScreenWidgetClass)
	{
		ShowMenuWidget(MainScreenWidgetClass);
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] MainScreen 표시"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] MainScreenWidgetClass가 설정되지 않았습니다."));
	}
}

void ABRPlayerController::HideMainScreen()
{
	if (MainScreenWidget && IsValid(MainScreenWidget))
	{
		MainScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] WBP_MainScreen 숨김"));
	}
}

void ABRPlayerController::SetMainScreenToEntranceMenu()
{
	// WBP_MainScreen 블루프린트에서 이 함수를 구현해야 합니다.
	if (MainScreenWidget && IsValid(MainScreenWidget))
	{
		UFunction* Function = MainScreenWidget->FindFunction(FName("SetMainScreenToEntranceMenu"));
		if (Function)
		{
			MainScreenWidget->ProcessEvent(Function, nullptr);
			UE_LOG(LogTemp, Log, TEXT("[PlayerController] SetMainScreenToEntranceMenu 호출 완료"));
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] SetMainScreenToEntranceMenu: MainScreenWidget이 없거나 함수를 찾을 수 없습니다."));
}

void ABRPlayerController::SetMainScreenToLobbyMenu()
{
	UWorld* World = GetWorld();
	// 클라이언트가 로비 맵에 도착한 뒤 한 번 더 이름 전송 → 서버에 ServerSetPlayerName 반영 (맵 이동 전 RPC 유실 대비)
	if (World && World->GetNetMode() == NM_Client && IsLocalController())
	{
		if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(World->GetGameInstance()))
		{
			FString Name = BRGI->GetPlayerName();
			if (!Name.IsEmpty())
			{
				SetPlayerName(Name);
				UE_LOG(LogTemp, Log, TEXT("[로비이름] 클라이언트 로비 진입 시 이름 재전송: '%s'"), *Name);
			}
		}
	}
	// Standalone 모드에서는 일반적으로 LobbyMenu를 표시하지 않지만,
	// 방 생성 후 ServerTravel로 인한 재로드인 경우는 예외
	if (World && World->GetNetMode() == NM_Standalone)
	{
		// 방 생성 후 재로드 상태인지 확인
		bool bIsRoomCreationReload = false;
		if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(World->GetGameInstance()))
		{
			bIsRoomCreationReload = BRGI->GetDidCreateRoomThenTravel();
		}
		
		// 플레이어가 있는지 확인 (세션 체크는 제거 - ServerTravel 직후 세션이 아직 초기화되지 않을 수 있음)
		bool bHasPlayers = false;
		if (ABRGameState* BRGameState = World->GetGameState<ABRGameState>())
		{
			bHasPlayers = BRGameState->PlayerArray.Num() > 0;
		}
		
		// 방 생성 후 재로드가 아니거나 플레이어가 없으면 LobbyMenu를 표시하지 않음
		if (!bIsRoomCreationReload || !bHasPlayers)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerController] SetMainScreenToLobbyMenu: Standalone 모드에서는 LobbyMenu를 표시하지 않습니다."));
			return;
		}
	}
	
	// WBP_MainScreen 블루프린트에서 이 함수를 구현해야 합니다.
	if (MainScreenWidget && IsValid(MainScreenWidget))
	{
		UFunction* Function = MainScreenWidget->FindFunction(FName("SetMainScreenToLobbyMenu"));
		if (Function)
		{
			MainScreenWidget->ProcessEvent(Function, nullptr);
			UE_LOG(LogTemp, Log, TEXT("[PlayerController] SetMainScreenToLobbyMenu 호출 완료"));
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] SetMainScreenToLobbyMenu: MainScreenWidget이 없거나 함수를 찾을 수 없습니다."));
}

void ABRPlayerController::HandleCreateRoomComplete(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("[PlayerController] 방 생성 완료: %s"), bWasSuccessful ? TEXT("성공") : TEXT("실패"));
	
	// 화면에 디버그 메시지 표시 (Standalone 모드에서도 확인 가능)
	if (GEngine)
	{
		FString Message = bWasSuccessful ? 
			TEXT("[방 생성] 성공! 리슨 서버로 전환 중...") : 
			TEXT("[방 생성] 실패! 다시 시도해주세요.");
		FColor MessageColor = bWasSuccessful ? FColor::Green : FColor::Red;
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, MessageColor, Message);
	}
	
	// 클라이언트에서만 UI 전환
	if (!IsLocalController())
	{
		return;
	}

	if (bWasSuccessful)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		ENetMode NetMode = World->GetNetMode();
		
		// Standalone 모드에서 방 생성 성공 시 ServerTravel이 호출되어 맵이 다시 로드되므로
		// 이 시점에서는 아직 Standalone 모드입니다. 맵이 다시 로드되면 BeginPlay에서 ListenServer 모드로 감지하여 LobbyMenu가 표시됩니다.
		// 하지만 이미 ListenServer 모드이거나 MainScreenWidget이 설정되어 있으면 즉시 LobbyMenu로 전환
		if (NetMode == NM_ListenServer)
		{
			// MainScreenWidget이 설정되어 있으면 MainScreen의 LobbyMenu로 전환
			if (MainScreenWidget && IsValid(MainScreenWidget))
			{
				SetMainScreenToLobbyMenu();
				UE_LOG(LogTemp, Log, TEXT("[PlayerController] 방 생성 성공 - LobbyMenu로 전환 (MainScreen)"));
			}
			// MainScreenWidget이 없으면 직접 LobbyMenu 위젯 표시
			else if (LobbyMenuWidgetClass)
			{
				ShowLobbyMenu();
				UE_LOG(LogTemp, Log, TEXT("[PlayerController] 방 생성 성공 - LobbyMenu로 전환"));
			}
		}
		// Standalone 모드에서는 ServerTravel이 호출되므로 여기서는 EntranceMenu 완전히 제거
		// (맵이 다시 로드되면 BeginPlay에서 ListenServer 모드로 감지하여 LobbyMenu가 표시됨)
		else if (NetMode == NM_Standalone)
		{
			// EntranceMenu 위젯 완전히 제거
			HideCurrentMenu();
			UE_LOG(LogTemp, Log, TEXT("[PlayerController] 방 생성 성공 - EntranceMenu 제거 (리슨 서버로 전환 대기 중...)"));
			
			// 화면에 추가 메시지 표시
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("맵 재로드 중... 잠시만 기다려주세요."));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerController] 방 생성 실패 - EntranceMenu 유지"));
		// 방 생성 실패 시에는 현재 화면(EntranceMenu)을 유지
	}
}

void ABRPlayerController::HideCurrentMenu()
{
	if (CurrentMenuWidget)
	{
		CurrentMenuWidget->RemoveFromParent();
		CurrentMenuWidget = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] 현재 메뉴 숨김"));
	}
}

void ABRPlayerController::LeaveRoom()
{
	UWorld* World = GetWorld();
	if (!World || !IsLocalController())
	{
		return;
	}

	ENetMode NetMode = World->GetNetMode();

	// 클라이언트: 서버 연결을 끊으면 서버에서 Logout(Exiting)이 호출되어 PlayerArray에서 제거됨
	if (NetMode == NM_Client)
	{
		ConsoleCommand(TEXT("disconnect"));
		UE_LOG(LogTemp, Log, TEXT("[방 나가기] 클라이언트: 서버 연결 종료 요청"));
		return;
	}

	// 호스트(ListenServer): 세션을 종료하고 메인 맵으로 이동 (모든 클라이언트도 함께 이동)
	if (NetMode == NM_ListenServer)
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
			{
				GameSession->DestroySessionAndReturnToMainMenu();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[방 나가기] 호스트: BRGameSession을 찾을 수 없습니다."));
			}
		}
		return;
	}

	// Standalone: UI만 전환 (연결 없음)
	if (NetMode == NM_Standalone)
	{
		SetMainScreenToEntranceMenu();
		UE_LOG(LogTemp, Log, TEXT("[방 나가기] Standalone: 입장 메뉴로 전환"));
	}
}

void ABRPlayerController::ShowMenuWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	// 클라이언트에서만 실행
	if (!IsLocalController())
	{
		return;
	}

	// WBP_MainScreen 숨기기 (HUD에서 관리되는 경우)
	if (MainScreenWidget && IsValid(MainScreenWidget))
	{
		MainScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] WBP_MainScreen 숨김 (HUD에서 관리)"));
	}

	// 현재 위젯이 있으면 제거
	if (CurrentMenuWidget)
	{
		CurrentMenuWidget->RemoveFromParent();
		CurrentMenuWidget = nullptr;
	}

	// 새 위젯 생성 및 표시
	if (WidgetClass)
	{
		// OwningPlayer를 명시적으로 전달 (Standalone 모드에서 입력 문제 해결)
		CurrentMenuWidget = CreateWidget<UUserWidget>(this, WidgetClass);
		if (CurrentMenuWidget)
		{
			CurrentMenuWidget->AddToViewport();
			// 입력 모드 설정 (UI에 포커스)
			FInputModeUIOnly InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputMode);
			bShowMouseCursor = true;
			
			// Standalone 모드에서 위젯이 입력을 받을 수 있도록 강제 설정
			if (CurrentMenuWidget->GetOwningPlayer() == nullptr)
			{
				CurrentMenuWidget->SetOwningPlayer(this);
				UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 위젯 OwningPlayer가 null이었습니다. 수동으로 설정했습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[PlayerController] 위젯 생성 실패"));
		}
	}
}