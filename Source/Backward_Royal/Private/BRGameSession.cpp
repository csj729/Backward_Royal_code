// BRGameSession.cpp
#include "BRGameSession.h"
#include "BRPlayerState.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ABRGameSession::ABRGameSession()
	: bIsSearchingSessions(false)
{
	// Online Subsystem 초기화
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		FString SubsystemName = OnlineSubsystem->GetSubsystemName().ToString();
		UE_LOG(LogTemp, Log, TEXT("[GameSession] Online Subsystem 초기화: %s"), *SubsystemName);
		
		SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[GameSession] SessionInterface 초기화 완료"));
			// 콜백 바인딩
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &ABRGameSession::OnCreateSessionCompleteDelegate);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &ABRGameSession::OnFindSessionsCompleteDelegate);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &ABRGameSession::OnJoinSessionCompleteDelegate);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameSession] SessionInterface 초기화 실패"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[GameSession] Online Subsystem 초기화 실패 - NULL"));
	}
}

void ABRGameSession::BeginPlay()
{
	Super::BeginPlay();
}

void ABRGameSession::CreateRoomSession(const FString& RoomName)
{
	UE_LOG(LogTemp, Log, TEXT("[방 생성] 세션 생성 시작: %s"), *RoomName);
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 생성] 실패: SessionInterface가 유효하지 않습니다."));
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	// 기존 세션이 있으면 제거
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[방 생성] 기존 세션 제거 중..."));
		SessionInterface->DestroySession(NAME_GameSession);
	}

	// 세션 설정 생성
	SessionSettings = MakeShareable(new FOnlineSessionSettings());
	SessionSettings->bIsLANMatch = true; // LAN 또는 Steam 등으로 변경 가능
	SessionSettings->NumPublicConnections = 8; // 최대 8명
	SessionSettings->NumPrivateConnections = 0;
	SessionSettings->bAllowInvites = true;
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bShouldAdvertise = true;
	SessionSettings->bUsesPresence = true;
	SessionSettings->bUseLobbiesIfAvailable = false;
	SessionSettings->Set(FName(TEXT("MAPNAME")), FString("Lobby"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	// 세션 이름 설정
	SessionSettings->Set(FName(TEXT("SESSION_NAME")), RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	UE_LOG(LogTemp, Log, TEXT("[방 생성] 세션 설정 완료: 최대 인원=%d, LAN 매치=%s"), 
		SessionSettings->NumPublicConnections,
		SessionSettings->bIsLANMatch ? TEXT("예") : TEXT("아니오"));

	// 세션 생성
	UE_LOG(LogTemp, Log, TEXT("[방 생성] 세션 생성 요청 전송 중..."));
	SessionInterface->CreateSession(0, NAME_GameSession, *SessionSettings);
}

void ABRGameSession::FindSessions()
{
	UE_LOG(LogTemp, Log, TEXT("[방 찾기] 세션 검색 시작"));
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] 실패: SessionInterface가 유효하지 않습니다."));
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>());
		return;
	}

	// 이미 검색 중이면 무시
	if (bIsSearchingSessions)
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 이미 검색이 진행 중입니다. 기다려주세요."));
		return;
	}

	// 기존 검색이 진행 중이면 취소
	if (SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[방 찾기] 기존 검색 취소 중..."));
		SessionInterface->CancelFindSessions();
		bIsSearchingSessions = false;
	}

	// 검색 시작 플래그 설정
	bIsSearchingSessions = true;

	// 세션 검색 설정
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->PingBucketSize = 50;
	SessionSearch->bIsLanQuery = true; // LAN 또는 Steam 등으로 변경 가능

	UE_LOG(LogTemp, Log, TEXT("[방 찾기] 검색 설정: 최대 결과=%d, LAN 검색=%s"), 
		SessionSearch->MaxSearchResults,
		SessionSearch->bIsLanQuery ? TEXT("예") : TEXT("아니오"));

	// 세션 찾기 시작
	UE_LOG(LogTemp, Log, TEXT("[방 찾기] 검색 요청 전송 중..."));
	bool bFindSessionsResult = SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	if (!bFindSessionsResult)
	{
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] FindSessions 호출 실패"));
		bIsSearchingSessions = false;
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>());
	}
}

void ABRGameSession::JoinSessionByIndex(int32 SessionIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[방 참가] 세션 인덱스: %d"), SessionIndex);
	
	if (!SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: 검색 결과가 없습니다. 먼저 FindRooms를 실행하세요."));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	if (SessionIndex < 0 || SessionIndex >= SessionSearch->SearchResults.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: 잘못된 세션 인덱스 (%d). 사용 가능한 범위: 0-%d"), 
			SessionIndex, SessionSearch->SearchResults.Num() - 1);
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	const FOnlineSessionSearchResult& SessionResult = SessionSearch->SearchResults[SessionIndex];
	JoinSession(SessionResult);
}

void ABRGameSession::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: SessionInterface가 유효하지 않습니다."));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	// 기존 세션이 있으면 제거 (다른 세션에 참가하기 전에)
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[방 참가] 기존 세션 제거 중... (다른 세션에 참가하기 위해)"));
		SessionInterface->DestroySession(NAME_GameSession);
		// DestroySession은 비동기이므로 잠시 대기 후 참가 시도
		// 실제로는 DestroySession 완료 콜백을 기다려야 하지만, 간단한 테스트를 위해 바로 시도
	}

	FString FoundSessionName;
	if (SessionResult.Session.SessionSettings.Get(FName(TEXT("SESSION_NAME")), FoundSessionName))
	{
		UE_LOG(LogTemp, Log, TEXT("[방 참가] 세션 참가 시도: %s"), *FoundSessionName);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[방 참가] 세션 참가 시도 중..."));
	}

	// 세션 참가
	SessionInterface->JoinSession(0, NAME_GameSession, SessionResult);
}

void ABRGameSession::OnCreateSessionCompleteDelegate(FName InSessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Log, TEXT("[방 생성] 성공: 세션이 생성되었습니다 - %s"), *InSessionName.ToString());
		UE_LOG(LogTemp, Log, TEXT("[방 생성] 방이 생성되었으며 다른 플레이어가 참가할 수 있습니다."));
		
		// 생성된 세션 정보 확인
		if (SessionInterface.IsValid())
		{
			auto Session = SessionInterface->GetNamedSession(NAME_GameSession);
			if (Session)
			{
				UE_LOG(LogTemp, Log, TEXT("[방 생성] 세션 정보: 최대 인원=%d, 현재 인원=%d, LAN=%s, Advertise=%s"), 
					Session->SessionSettings.NumPublicConnections,
					Session->NumOpenPublicConnections,
					Session->SessionSettings.bIsLANMatch ? TEXT("예") : TEXT("아니오"),
					Session->SessionSettings.bShouldAdvertise ? TEXT("예") : TEXT("아니오"));
			}
		}

		// 리슨 서버로 시작하기 위해 현재 맵을 ?listen 옵션과 함께 다시 로드
		UWorld* World = GetWorld();
		if (World)
		{
			ENetMode NetMode = World->GetNetMode();
			UE_LOG(LogTemp, Log, TEXT("[방 생성] 현재 네트워크 모드: %s"), 
				NetMode == NM_Standalone ? TEXT("Standalone") :
				NetMode == NM_ListenServer ? TEXT("ListenServer") :
				NetMode == NM_Client ? TEXT("Client") :
				NetMode == NM_DedicatedServer ? TEXT("DedicatedServer") : TEXT("Unknown"));
			
			// Standalone 모드인 경우에만 리슨 서버로 전환
			if (NetMode == NM_Standalone)
			{
				// 현재 맵 경로 가져오기
				FString CurrentMapName = UGameplayStatics::GetCurrentLevelName(World, true);
				
				// 맵 경로가 비어있으면 GetMapName() 사용
				if (CurrentMapName.IsEmpty())
				{
					CurrentMapName = World->GetMapName();
					CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);
				}
				
				// 맵 경로 구성 (현재 맵을 리슨 서버로 다시 로드)
				FString ListenURL = FString::Printf(TEXT("%s?listen"), *CurrentMapName);
				
				UE_LOG(LogTemp, Log, TEXT("[방 생성] 리슨 서버로 전환 중: %s"), *ListenURL);
				UE_LOG(LogTemp, Log, TEXT("[방 생성] 클라이언트 연결 대기 중..."));
				
				// 리슨 서버로 전환
				World->ServerTravel(ListenURL);
			}
			else if (NetMode == NM_ListenServer)
			{
				UE_LOG(LogTemp, Log, TEXT("[방 생성] 이미 리슨 서버 모드입니다. 클라이언트 연결 대기 중..."));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[방 생성] 리슨 서버로 전환할 수 없습니다. 현재 모드: %s"), 
					NetMode == NM_Client ? TEXT("Client") : TEXT("DedicatedServer"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[방 생성] 실패: 세션 생성에 실패했습니다."));
	}

	OnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void ABRGameSession::OnFindSessionsCompleteDelegate(bool bWasSuccessful)
{
	// 검색이 진행 중이 아니면 무시 (중복 콜백 방지)
	// 단, 첫 번째 콜백이 이미 처리되었을 수 있으므로 조용히 무시
	if (!bIsSearchingSessions)
	{
		// 이미 처리된 콜백이므로 조용히 무시 (로그 출력 안 함)
		return;
	}

	// 검색 완료 플래그 해제
	bIsSearchingSessions = false;

	TArray<FOnlineSessionSearchResult> Results;
	
	UE_LOG(LogTemp, Log, TEXT("[방 찾기] 콜백 호출: bWasSuccessful=%s, SessionSearch.IsValid()=%s"), 
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		SessionSearch.IsValid() ? TEXT("true") : TEXT("false"));
	
	if (bWasSuccessful)
	{
		if (SessionSearch.IsValid())
		{
			Results = SessionSearch->SearchResults;
			UE_LOG(LogTemp, Log, TEXT("[방 찾기] 성공: 찾은 세션 수 = %d"), Results.Num());
			if (Results.Num() > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("[방 찾기] 참가하려면 'JoinRoom [인덱스]' 명령어를 사용하세요."));
				for (int32 i = 0; i < Results.Num(); i++)
				{
					const FOnlineSessionSearchResult& Result = Results[i];
					FString FoundSessionName;
					FString FoundMapName;
					int32 CurrentPlayerCount = 0;
					int32 MaxPlayerCount = 0;
					
					// 세션 정보 추출
					if (Result.Session.SessionSettings.Get(FName(TEXT("SESSION_NAME")), FoundSessionName))
					{
						UE_LOG(LogTemp, Log, TEXT("[방 찾기] 세션 [%d]: 이름=%s, Ping=%dms"), i, *FoundSessionName, Result.PingInMs);
					}
					else
					{
						UE_LOG(LogTemp, Log, TEXT("[방 찾기] 세션 [%d]: 이름=(없음), Ping=%dms"), i, Result.PingInMs);
					}
					
					if (Result.Session.SessionSettings.Get(FName(TEXT("MAPNAME")), FoundMapName))
					{
						UE_LOG(LogTemp, Log, TEXT("[방 찾기] 세션 [%d]: 맵=%s"), i, *FoundMapName);
					}
					
					CurrentPlayerCount = Result.Session.NumOpenPublicConnections + Result.Session.NumOpenPrivateConnections;
					MaxPlayerCount = Result.Session.SessionSettings.NumPublicConnections + Result.Session.SessionSettings.NumPrivateConnections;
					UE_LOG(LogTemp, Log, TEXT("[방 찾기] 세션 [%d]: 플레이어 수=%d/%d"), i, MaxPlayerCount - CurrentPlayerCount, MaxPlayerCount);
					UE_LOG(LogTemp, Log, TEXT("[방 찾기] 세션 [%d] 참가 명령: JoinRoom %d"), i, i);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 사용 가능한 세션이 없습니다."));
				UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 참고: 같은 프로세스에서 생성한 세션은 검색되지 않을 수 있습니다."));
				UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 참고: 다른 게임 인스턴스를 실행하여 테스트해보세요."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[방 찾기] 실패: SessionSearch가 유효하지 않습니다. (bWasSuccessful=true이지만)"));
			UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 참고: 이는 비동기 타이밍 문제일 수 있습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] 실패: 세션 찾기에 실패했습니다. (bWasSuccessful=false)"));
	}

	OnFindSessionsComplete.Broadcast(Results);
}

void ABRGameSession::OnJoinSessionCompleteDelegate(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	bool bWasSuccessful = (Result == EOnJoinSessionCompleteResult::Success);
	
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Log, TEXT("[방 참가] 성공: 세션 참가 완료 - %s"), *InSessionName.ToString());
		
		// 서버로 여행 (클라이언트만)
		if (SessionInterface.IsValid())
		{
			FString TravelURL;
			bool bGotConnectString = SessionInterface->GetResolvedConnectString(NAME_GameSession, TravelURL);
			
			// 포트가 0이면 기본 포트 7777로 변경
			if (bGotConnectString && TravelURL.Contains(TEXT(":0")))
			{
				FString IP;
				if (TravelURL.Split(TEXT(":"), &IP, nullptr))
				{
					TravelURL = FString::Printf(TEXT("%s:7777"), *IP);
					UE_LOG(LogTemp, Log, TEXT("[방 참가] 포트가 0이므로 기본 포트 7777로 변경: %s"), *TravelURL);
				}
			}
			
			if (!TravelURL.IsEmpty())
			{
				UWorld* World = GetWorld();
				if (World)
				{
					ENetMode NetMode = World->GetNetMode();
					
					// ListenServer나 DedicatedServer가 아닌 경우에만 서버로 이동
					// (Standalone 모드에서 다른 세션에 참가할 때는 ClientTravel 실행)
					if (NetMode != NM_ListenServer && NetMode != NM_DedicatedServer)
					{
						UE_LOG(LogTemp, Log, TEXT("[방 참가] 서버로 이동 중: %s (네트워크 모드: %s)"), 
							*TravelURL, 
							NetMode == NM_Client ? TEXT("Client") : 
							NetMode == NM_Standalone ? TEXT("Standalone") : TEXT("Unknown"));
						
						// 첫 번째 플레이어 컨트롤러에게만 여행 명령 전송
						if (APlayerController* PC = World->GetFirstPlayerController())
						{
							PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
						}
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[방 참가] 호스트는 이미 서버이므로 다른 방에 참가할 수 없습니다."));
						UE_LOG(LogTemp, Warning, TEXT("[방 참가] 클라이언트만 다른 방에 참가할 수 있습니다."));
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: 연결 문자열을 가져올 수 없습니다."));
			}
		}
	}
	else
	{
		FString ErrorMessage;
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::Success:
			ErrorMessage = TEXT("성공");
			break;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			ErrorMessage = TEXT("주소를 가져올 수 없음");
			break;
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			ErrorMessage = TEXT("이미 세션에 참가 중입니다. 기존 세션을 먼저 떠나야 합니다.");
			break;
		case EOnJoinSessionCompleteResult::SessionIsFull:
			ErrorMessage = TEXT("세션이 가득 찼습니다");
			break;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			ErrorMessage = TEXT("세션이 존재하지 않습니다");
			break;
		case EOnJoinSessionCompleteResult::UnknownError:
		default:
			ErrorMessage = TEXT("알 수 없는 오류");
			break;
		}
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: 세션 참가에 실패했습니다. (결과 코드: %d, %s)"), (int32)Result, *ErrorMessage);
		
		if (Result == EOnJoinSessionCompleteResult::AlreadyInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("[방 참가] 참고: 같은 프로세스에서 생성한 방에 참가할 수 없습니다."));
			UE_LOG(LogTemp, Warning, TEXT("[방 참가] 참고: 다른 게임 인스턴스를 실행하여 테스트해보세요."));
		}
	}

	OnJoinSessionComplete.Broadcast(bWasSuccessful);
}

