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
	// 생성자에서는 초기화하지 않음 (BeginPlay에서 초기화)
	// Standalone 모드에서는 생성자 시점에 World가 준비되지 않을 수 있음
}

void ABRGameSession::InitializeOnlineSubsystem()
{
	// Online Subsystem 초기화
	// Steam을 우선적으로 시도, 실패하면 Null로 폴백
	IOnlineSubsystem* OnlineSubsystem = nullptr;
	
	// 방법 1: Steam 시도 (우선순위)
	UE_LOG(LogTemp, Warning, TEXT("[GameSession] Steam Online Subsystem 초기화 시도 중..."));
	OnlineSubsystem = IOnlineSubsystem::Get(FName("Steam"));
	if (OnlineSubsystem)
	{
		FString SubsystemName = OnlineSubsystem->GetSubsystemName().ToString();
		UE_LOG(LogTemp, Warning, TEXT("[GameSession] IOnlineSubsystem::Get(Steam) 성공! SubsystemName: %s"), *SubsystemName);
		if (GEngine)
		{
			FString SuccessMsg = FString::Printf(TEXT("[GameSession] Steam Online Subsystem 초기화 성공! (%s)"), *SubsystemName);
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, SuccessMsg);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[GameSession] IOnlineSubsystem::Get(Steam) 실패 - NULL 반환"));
		UE_LOG(LogTemp, Error, TEXT("[GameSession] 가능한 원인:"));
		UE_LOG(LogTemp, Error, TEXT("  1. Steam 클라이언트가 실행되지 않음"));
		UE_LOG(LogTemp, Error, TEXT("  2. Steam SDK가 설치되지 않음"));
		UE_LOG(LogTemp, Error, TEXT("  3. Steam App ID가 설정되지 않음"));
		UE_LOG(LogTemp, Error, TEXT("  4. Standalone 모드에서 Steam 플러그인이 로드되지 않음"));
		
		if (GEngine)
		{
			FString ErrorMsg = TEXT("[GameSession] Steam 초기화 실패!\n");
			ErrorMsg += TEXT("확인 사항:\n");
			ErrorMsg += TEXT("1. Steam 클라이언트 실행 중인지 확인\n");
			ErrorMsg += TEXT("2. Steam에 로그인되어 있는지 확인\n");
			ErrorMsg += TEXT("3. Steam SDK 설치 여부 확인");
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, ErrorMsg);
		}
		
		// 방법 2: Null로 폴백
		OnlineSubsystem = IOnlineSubsystem::Get(FName("Null"));
		if (OnlineSubsystem)
		{
			UE_LOG(LogTemp, Log, TEXT("[GameSession] IOnlineSubsystem::Get(Null) 성공 (Steam 폴백)"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("[GameSession] Steam 실패, Null Online Subsystem 사용 중"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameSession] IOnlineSubsystem::Get(Null) 실패"));
			
			// 방법 3: 기본값으로 시도
			OnlineSubsystem = IOnlineSubsystem::Get();
			if (OnlineSubsystem)
			{
				UE_LOG(LogTemp, Log, TEXT("[GameSession] IOnlineSubsystem::Get() (기본값) 성공"));
			}
		}
	}
	
	if (OnlineSubsystem)
	{
		FString SubsystemName = OnlineSubsystem->GetSubsystemName().ToString();
		UE_LOG(LogTemp, Log, TEXT("[GameSession] Online Subsystem 초기화: %s"), *SubsystemName);
		
		// 화면에 메시지 표시
		if (GEngine)
		{
			FString Message = FString::Printf(TEXT("[GameSession] Online Subsystem: %s"), *SubsystemName);
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Message);
		}
		
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
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[GameSession] SessionInterface 초기화 실패!"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[GameSession] Online Subsystem 초기화 실패 - NULL"));
		if (GEngine)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				ENetMode NetMode = World->GetNetMode();
				if (NetMode == NM_Standalone)
				{
					FString WarningMsg = TEXT("[GameSession] Standalone 모드: Online Subsystem이 NULL입니다.\n");
					WarningMsg += TEXT("세션 기능을 사용하려면 Listen Server 모드를 사용하세요.\n");
					WarningMsg += TEXT("(Play 버튼 옆 드롭다운 -> Number of Players: 2+)");
					GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, WarningMsg);
					UE_LOG(LogTemp, Warning, TEXT("%s"), *WarningMsg);
				}
				else
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[GameSession] Online Subsystem 초기화 실패!"));
				}
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[GameSession] Online Subsystem 초기화 실패!"));
			}
		}
	}
}

void ABRGameSession::BeginPlay()
{
	Super::BeginPlay();
	
	// BeginPlay에서 Online Subsystem 초기화 (Standalone 모드에서도 작동하도록)
	// 약간의 지연 후 초기화 (모든 시스템이 준비된 후)
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
	{
		InitializeOnlineSubsystem();
	}, 0.1f, false);
}

void ABRGameSession::CreateRoomSession(const FString& RoomName)
{
	UE_LOG(LogTemp, Log, TEXT("[방 생성] 세션 생성 시작: %s"), *RoomName);
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 생성] 실패: SessionInterface가 유효하지 않습니다."));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 생성] 실패: SessionInterface가 유효하지 않습니다!"));
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Online Subsystem이 초기화되지 않았을 수 있습니다."));
		}
		
		// SessionInterface가 없으면 다시 초기화 시도
		InitializeOnlineSubsystem();
		
		// 초기화 후에도 유효하지 않으면 실패
		if (!SessionInterface.IsValid())
		{
			OnCreateSessionComplete.Broadcast(false);
			return;
		}
	}

	// 기존 세션이 있으면 제거
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] 기존 세션이 발견되었습니다. 제거 중..."));
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] 기존 세션 정보: 최대 인원=%d, 현재 인원=%d"), 
			ExistingSession->SessionSettings.NumPublicConnections,
			ExistingSession->NumOpenPublicConnections);
		
		// DestroySession은 비동기이므로 완료를 기다려야 할 수 있지만,
		// CreateSession이 실패하는 경우가 있으므로 즉시 제거 시도
		bool bDestroyResult = SessionInterface->DestroySession(NAME_GameSession);
		if (bDestroyResult)
		{
			UE_LOG(LogTemp, Warning, TEXT("[방 생성] 기존 세션 제거 요청 전송됨 (비동기 처리 중...)"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[방 생성] 기존 세션 제거 실패! 세션이 이미 제거되었을 수 있습니다."));
		}
		
		// 잠시 대기 (세션 제거 완료 대기)
		// 참고: DestroySession이 완료될 때까지 기다리는 것이 이상적이지만,
		// 현재는 즉시 CreateSession을 시도합니다
	}

	// 세션 설정 생성
	SessionSettings = MakeShareable(new FOnlineSessionSettings());
	
	// Steam을 사용할 때는 bIsLANMatch를 false로 설정해야 함
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	FString SubsystemName = OnlineSubsystem ? OnlineSubsystem->GetSubsystemName().ToString() : TEXT("Unknown");
	bool bIsSteam = SubsystemName.Equals(TEXT("Steam"), ESearchCase::IgnoreCase);
	
	SessionSettings->bIsLANMatch = !bIsSteam; // Steam이면 false, Null이면 true
	SessionSettings->NumPublicConnections = 8; // 최대 8명
	SessionSettings->NumPrivateConnections = 0;
	SessionSettings->bAllowInvites = true;
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bShouldAdvertise = true;
	SessionSettings->bUsesPresence = true;
	// Steam presence 검색/참가에 필요 (방 찾기 시 SEARCH_PRESENCE와 쌍을 이룸)
	if (bIsSteam)
	{
		SessionSettings->bAllowJoinViaPresence = true;
	}
	// Steam에서는 Lobby를 사용하는 것이 더 안정적입니다
	// Lobby를 사용하지 않으면 Steam 세션 생성이 실패할 수 있습니다
	SessionSettings->bUseLobbiesIfAvailable = bIsSteam; // Steam이면 Lobby 사용
	
	UE_LOG(LogTemp, Warning, TEXT("[방 생성] 세션 설정: Subsystem=%s, bIsLANMatch=%s, bUseLobbiesIfAvailable=%s"), 
		*SubsystemName,
		SessionSettings->bIsLANMatch ? TEXT("true") : TEXT("false"),
		SessionSettings->bUseLobbiesIfAvailable ? TEXT("true") : TEXT("false"));
	SessionSettings->Set(FName(TEXT("MAPNAME")), FString("Lobby"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	// 세션 이름 설정
	SessionSettings->Set(FName(TEXT("SESSION_NAME")), RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	UE_LOG(LogTemp, Log, TEXT("[방 생성] 세션 설정 완료: 최대 인원=%d, LAN 매치=%s"), 
		SessionSettings->NumPublicConnections,
		SessionSettings->bIsLANMatch ? TEXT("예") : TEXT("아니오"));

	// 세션 생성
	UE_LOG(LogTemp, Warning, TEXT("[방 생성] 세션 생성 요청 전송 중..."));
	UE_LOG(LogTemp, Warning, TEXT("[방 생성] 세션 설정 요약:"));
	UE_LOG(LogTemp, Warning, TEXT("  - Subsystem: %s"), *SubsystemName);
	UE_LOG(LogTemp, Warning, TEXT("  - bIsLANMatch: %s"), SessionSettings->bIsLANMatch ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - bUseLobbiesIfAvailable: %s"), SessionSettings->bUseLobbiesIfAvailable ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - bShouldAdvertise: %s"), SessionSettings->bShouldAdvertise ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - bUsesPresence: %s"), SessionSettings->bUsesPresence ? TEXT("true") : TEXT("false"));
	
	if (GEngine)
	{
		FString DebugMsg = FString::Printf(TEXT("[방 생성] 세션 생성 요청 중...\nSubsystem: %s\nLAN: %s"), 
			*SubsystemName,
			SessionSettings->bIsLANMatch ? TEXT("Yes") : TEXT("No"));
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugMsg);
	}
	
	// Steam 세션 생성 전 추가 검증
	if (bIsSteam)
	{
		// Steam 클라이언트 확인
		if (!OnlineSubsystem || OnlineSubsystem->GetSubsystemName() != FName("Steam"))
		{
			UE_LOG(LogTemp, Error, TEXT("[방 생성] Steam OnlineSubsystem이 올바르게 초기화되지 않았습니다!"));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("[방 생성] Steam 초기화 실패!\nSteam 클라이언트를 확인하세요."));
			}
			OnCreateSessionComplete.Broadcast(false);
			return;
		}
		
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] Steam 세션 생성 시도 중..."));
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] Steam 세션 설정 확인:"));
		UE_LOG(LogTemp, Warning, TEXT("  - bIsLANMatch: %s"), SessionSettings->bIsLANMatch ? TEXT("true") : TEXT("false"));
		UE_LOG(LogTemp, Warning, TEXT("  - bUseLobbiesIfAvailable: %s"), SessionSettings->bUseLobbiesIfAvailable ? TEXT("true") : TEXT("false"));
		UE_LOG(LogTemp, Warning, TEXT("  - bUsesPresence: %s"), SessionSettings->bUsesPresence ? TEXT("true") : TEXT("false"));
		UE_LOG(LogTemp, Warning, TEXT("  - bShouldAdvertise: %s"), SessionSettings->bShouldAdvertise ? TEXT("true") : TEXT("false"));
	}
	
	bool bCreateResult = SessionInterface->CreateSession(0, NAME_GameSession, *SessionSettings);
	if (!bCreateResult)
	{
		UE_LOG(LogTemp, Error, TEXT("[방 생성] CreateSession 호출이 즉시 실패했습니다!"));
		
		// Steam 관련 추가 정보
		if (bIsSteam)
		{
			UE_LOG(LogTemp, Error, TEXT("[방 생성] Steam 세션 생성 즉시 실패 - 가능한 원인:"));
			UE_LOG(LogTemp, Error, TEXT("  1. Steam 클라이언트가 실행되지 않음"));
			UE_LOG(LogTemp, Error, TEXT("  2. Steam에 로그인되지 않음"));
			UE_LOG(LogTemp, Error, TEXT("  3. Steam 네트워크 연결 문제"));
			UE_LOG(LogTemp, Error, TEXT("  4. Steam App ID 설정 문제"));
			UE_LOG(LogTemp, Error, TEXT("  5. 이미 다른 세션이 활성화되어 있음"));
			
			if (GEngine)
			{
				FString ErrorMsg = TEXT("[방 생성] Steam 세션 생성 실패!\n");
				ErrorMsg += TEXT("확인 사항:\n");
				ErrorMsg += TEXT("1. Steam 클라이언트 실행 중인지 확인\n");
				ErrorMsg += TEXT("2. Steam에 로그인되어 있는지 확인\n");
				ErrorMsg += TEXT("3. Steam 네트워크 연결 확인\n");
				ErrorMsg += TEXT("4. 기존 세션이 있는지 확인");
				GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, ErrorMsg);
			}
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 생성] CreateSession 호출 실패!"));
			}
		}
		
		OnCreateSessionComplete.Broadcast(false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] CreateSession 호출 성공 (비동기 처리 대기 중...)"));
		if (GEngine && bIsSteam)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("[방 생성] Steam 세션 생성 요청 전송됨..."));
		}
	}
}

void ABRGameSession::FindSessions()
{
	UE_LOG(LogTemp, Log, TEXT("[방 찾기] 세션 검색 시작"));
	
	// 화면에 디버그 메시지 표시
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("[방 찾기] 세션 검색 시작..."));
	}
	
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] 실패: SessionInterface가 유효하지 않습니다."));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 찾기] 실패: SessionInterface가 유효하지 않습니다!"));
		}
		
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>());
		return;
	}

	// 이미 검색 중이면 무시
	if (bIsSearchingSessions)
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 이미 검색이 진행 중입니다. 기다려주세요."));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("[방 찾기] 이미 검색이 진행 중입니다. 기다려주세요."));
		}
		
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
	
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	FString SubsystemName = OnlineSubsystem ? OnlineSubsystem->GetSubsystemName().ToString() : TEXT("Unknown");
	bool bIsSteam = SubsystemName.Equals(TEXT("Steam"), ESearchCase::IgnoreCase);
	
	// Steam: 동일 LAN에서 방이 LAN 목록에 뜨는 경우가 많음. bIsLanQuery=true로 검색.
	// Null: LAN 전용.
	SessionSearch->bIsLanQuery = true;
	
	if (bIsSteam)
	{
		// 최소 1개 QuerySettings 필요(검색 미실행 방지). PRESENCESEARCH = bUsesPresence와 쌍.
		SessionSearch->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);
		UE_LOG(LogTemp, Warning, TEXT("[방 찾기] Steam: bIsLanQuery=true, PRESENCESEARCH=true"));
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 검색 설정: Subsystem=%s, bIsLanQuery=%s"), 
		*SubsystemName,
		SessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"));

	UE_LOG(LogTemp, Log, TEXT("[방 찾기] 검색 설정: 최대 결과=%d, LAN 검색=%s"), 
		SessionSearch->MaxSearchResults,
		SessionSearch->bIsLanQuery ? TEXT("예") : TEXT("아니오"));

	// 세션 찾기 시작
	UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 검색 요청 전송 중..."));
	UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 검색 설정 요약:"));
	UE_LOG(LogTemp, Warning, TEXT("  - Subsystem: %s"), *SubsystemName);
	UE_LOG(LogTemp, Warning, TEXT("  - bIsLanQuery: %s"), SessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - MaxSearchResults: %d"), SessionSearch->MaxSearchResults);
	
	if (GEngine)
	{
		FString DebugMsg = FString::Printf(TEXT("[방 찾기] 검색 요청 중...\nSubsystem: %s\nLAN: %s"), 
			*SubsystemName,
			SessionSearch->bIsLanQuery ? TEXT("Yes") : TEXT("No"));
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugMsg);
	}
	
	bool bFindSessionsResult = SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	if (!bFindSessionsResult)
	{
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] FindSessions 호출이 즉시 실패했습니다!"));
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] 가능한 원인:"));
		UE_LOG(LogTemp, Error, TEXT("  1. SessionInterface가 유효하지 않음"));
		UE_LOG(LogTemp, Error, TEXT("  2. Steam 네트워크 연결 문제"));
		UE_LOG(LogTemp, Error, TEXT("  3. 이미 검색이 진행 중"));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			FString ErrorMsg = TEXT("[방 찾기] FindSessions 호출 실패!\n");
			ErrorMsg += FString::Printf(TEXT("Subsystem: %s\n"), *SubsystemName);
			ErrorMsg += TEXT("Steam 네트워크 연결 확인 필요");
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, ErrorMsg);
		}
		
		bIsSearchingSessions = false;
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>());
		OnFindSessionsCompleteBP.Broadcast(0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 찾기] FindSessions 호출 성공 (비동기 처리 대기 중...)"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("[방 찾기] 검색 중... 잠시만 기다려주세요."));
		}
	}
}

void ABRGameSession::JoinSessionByIndex(int32 SessionIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("[방 참가] JoinSessionByIndex 호출됨: 세션 인덱스=%d"), SessionIndex);
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	
	// SessionInterface 유효성 확인 (Standalone 모드에서 NULL일 수 있음)
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: SessionInterface가 NULL입니다. Online Subsystem이 초기화되지 않았습니다."));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			FString ErrorMsg = TEXT("[방 참가] 실패: Online Subsystem이 NULL입니다!\n");
			ErrorMsg += TEXT("Standalone 모드에서는 세션 기능을 사용할 수 없습니다.\n");
			ErrorMsg += TEXT("Listen Server 모드를 사용하세요 (Number of Players: 2+)");
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, ErrorMsg);
		}
		
		OnJoinSessionComplete.Broadcast(false);
		return;
	}
	
	if (!SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: 검색 결과가 없습니다. 먼저 FindRooms를 실행하세요."));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 참가] 실패: 검색 결과가 없습니다! 먼저 방 찾기를 실행하세요."));
		}
		
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	if (SessionIndex < 0 || SessionIndex >= SessionSearch->SearchResults.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: 잘못된 세션 인덱스 (%d). 사용 가능한 범위: 0-%d"), 
			SessionIndex, SessionSearch->SearchResults.Num() - 1);
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			FString ErrorMsg = FString::Printf(TEXT("[방 참가] 실패: 잘못된 세션 인덱스 (%d). 범위: 0-%d"), 
				SessionIndex, SessionSearch->SearchResults.Num() - 1);
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, ErrorMsg);
		}
		
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
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 참가] 실패: SessionInterface가 유효하지 않습니다!"));
		}
		
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	// 기존 세션이 있으면 제거 (다른 세션에 참가하기 전에)
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[방 참가] 기존 세션 제거 중... (다른 세션에 참가하기 위해)"));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("[방 참가] 기존 세션 제거 중..."));
		}
		
		SessionInterface->DestroySession(NAME_GameSession);
		// DestroySession은 비동기이므로 잠시 대기 후 참가 시도
		// 실제로는 DestroySession 완료 콜백을 기다려야 하지만, 간단한 테스트를 위해 바로 시도
	}

	FString FoundSessionName;
	if (SessionResult.Session.SessionSettings.Get(FName(TEXT("SESSION_NAME")), FoundSessionName))
	{
		UE_LOG(LogTemp, Log, TEXT("[방 참가] 세션 참가 시도: %s"), *FoundSessionName);
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			FString Message = FString::Printf(TEXT("[방 참가] 세션 참가 시도: %s"), *FoundSessionName);
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Message);
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[방 참가] 세션 참가 시도 중..."));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("[방 참가] 세션 참가 시도 중..."));
		}
	}

	// 세션 참가
	UE_LOG(LogTemp, Warning, TEXT("[방 참가] SessionInterface->JoinSession() 호출 중..."));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("[방 참가] SessionInterface->JoinSession() 호출 중..."));
	}
	
	SessionInterface->JoinSession(0, NAME_GameSession, SessionResult);
	
	UE_LOG(LogTemp, Warning, TEXT("[방 참가] SessionInterface->JoinSession() 호출 완료 (비동기 처리 중)"));
}

void ABRGameSession::OnCreateSessionCompleteDelegate(FName InSessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Log, TEXT("[방 생성] 성공: 세션이 생성되었습니다 - %s"), *InSessionName.ToString());
		UE_LOG(LogTemp, Log, TEXT("[방 생성] 방이 생성되었으며 다른 플레이어가 참가할 수 있습니다."));
		
		// 화면에 디버그 메시지 표시 (Standalone 모드에서도 확인 가능)
		if (GEngine)
		{
			FString Message = FString::Printf(TEXT("[GameSession] 방 생성 성공! 세션: %s"), *InSessionName.ToString());
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, Message);
		}
		
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
		UE_LOG(LogTemp, Error, TEXT("========================================"));
		UE_LOG(LogTemp, Error, TEXT("[방 생성] 실패: 세션 생성에 실패했습니다."));
		UE_LOG(LogTemp, Error, TEXT("세션 이름: %s"), *InSessionName.ToString());
		
		// 실패 원인 확인
		if (SessionInterface.IsValid())
		{
			IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
			FString SubsystemName = OnlineSubsystem ? OnlineSubsystem->GetSubsystemName().ToString() : TEXT("Unknown");
			UE_LOG(LogTemp, Error, TEXT("Online Subsystem: %s"), *SubsystemName);
			
			// Steam 관련 추가 정보
			if (SubsystemName.Equals(TEXT("Steam"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogTemp, Error, TEXT("Steam 세션 생성 실패 가능 원인:"));
				UE_LOG(LogTemp, Error, TEXT("  1. Steam 클라이언트가 실행되지 않음"));
				UE_LOG(LogTemp, Error, TEXT("  2. Steam에 로그인되지 않음"));
				UE_LOG(LogTemp, Error, TEXT("  3. Steam 네트워크 연결 문제"));
				UE_LOG(LogTemp, Error, TEXT("  4. Steam App ID 설정 문제"));
			}
		}
		UE_LOG(LogTemp, Error, TEXT("========================================"));
		
		// 화면에 디버그 메시지 표시 (Standalone 모드에서도 확인 가능)
		if (GEngine)
		{
			FString ErrorMsg = TEXT("[GameSession] 방 생성 실패!\n");
			if (SessionInterface.IsValid())
			{
				IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
				FString SubsystemName = OnlineSubsystem ? OnlineSubsystem->GetSubsystemName().ToString() : TEXT("Unknown");
				ErrorMsg += FString::Printf(TEXT("Subsystem: %s\n"), *SubsystemName);
				
				if (SubsystemName.Equals(TEXT("Steam"), ESearchCase::IgnoreCase))
				{
					ErrorMsg += TEXT("Steam 세션 생성 실패\n");
					ErrorMsg += TEXT("Steam 클라이언트 확인 필요");
				}
			}
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, ErrorMsg);
		}
	}

	OnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void ABRGameSession::OnFindSessionsCompleteDelegate(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("[방 찾기] OnFindSessionsCompleteDelegate 호출됨"));
	UE_LOG(LogTemp, Warning, TEXT("  - bWasSuccessful: %s"), bWasSuccessful ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - bIsSearchingSessions: %s"), bIsSearchingSessions ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  - SessionSearch.IsValid(): %s"), SessionSearch.IsValid() ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	
	// 검색이 진행 중이 아니면 무시 (중복 콜백 방지)
	// 단, 첫 번째 콜백이 이미 처리되었을 수 있으므로 조용히 무시
	if (!bIsSearchingSessions)
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 이미 처리된 콜백이므로 무시합니다."));
		// 이미 처리된 콜백이므로 조용히 무시 (로그 출력 안 함)
		return;
	}

	// 검색 완료 플래그 해제
	bIsSearchingSessions = false;

	// OnlineSubsystem을 함수 상단에서 한 번만 가져옴 (중복 선언 방지)
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	FString SubsystemName = OnlineSubsystem ? OnlineSubsystem->GetSubsystemName().ToString() : TEXT("Unknown");

	TArray<FOnlineSessionSearchResult> Results;
	
	if (bWasSuccessful)
	{
		if (SessionSearch.IsValid())
		{
			Results = SessionSearch->SearchResults;
			UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 성공: 찾은 세션 수 = %d"), Results.Num());
			
			// Steam 세션 검색 시 추가 정보
			UE_LOG(LogTemp, Warning, TEXT("[방 찾기] Online Subsystem: %s"), *SubsystemName);
			
			if (SubsystemName.Equals(TEXT("Steam"), ESearchCase::IgnoreCase))
			{
				if (Results.Num() == 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("[방 찾기] Steam 세션을 찾지 못했습니다."));
					UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 가능한 원인:"));
					UE_LOG(LogTemp, Warning, TEXT("  1. 방이 아직 생성되지 않음"));
					UE_LOG(LogTemp, Warning, TEXT("  2. 방 생성 시 bShouldAdvertise=false로 설정됨"));
					UE_LOG(LogTemp, Warning, TEXT("  3. Steam 네트워크 문제"));
					UE_LOG(LogTemp, Warning, TEXT("  4. 방 생성과 방 찾기의 세션 설정이 일치하지 않음"));
				}
			}
			
			// 화면에 디버그 메시지 표시
			if (GEngine)
			{
				FString Message = FString::Printf(TEXT("[방 찾기] 완료: %d개의 방을 찾았습니다."), Results.Num());
				FColor MessageColor = Results.Num() > 0 ? FColor::Green : FColor::Yellow;
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, MessageColor, Message);
			}
			
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
						
						// 화면에 세션 정보 표시
						if (GEngine)
						{
							CurrentPlayerCount = Result.Session.NumOpenPublicConnections + Result.Session.NumOpenPrivateConnections;
							MaxPlayerCount = Result.Session.SessionSettings.NumPublicConnections + Result.Session.SessionSettings.NumPrivateConnections;
							int32 ActualPlayerCount = MaxPlayerCount - CurrentPlayerCount;
							FString SessionInfo = FString::Printf(TEXT("  [%d] %s - 플레이어: %d/%d, Ping: %dms"), 
								i, *FoundSessionName, ActualPlayerCount, MaxPlayerCount, Result.PingInMs);
							GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, SessionInfo);
						}
					}
					else
					{
						UE_LOG(LogTemp, Log, TEXT("[방 찾기] 세션 [%d]: 이름=(없음), Ping=%dms"), i, Result.PingInMs);
						
						// 화면에 세션 정보 표시 (이름 없음)
						if (GEngine)
						{
							CurrentPlayerCount = Result.Session.NumOpenPublicConnections + Result.Session.NumOpenPrivateConnections;
							MaxPlayerCount = Result.Session.SessionSettings.NumPublicConnections + Result.Session.SessionSettings.NumPrivateConnections;
							int32 ActualPlayerCount = MaxPlayerCount - CurrentPlayerCount;
							FString SessionInfo = FString::Printf(TEXT("  [%d] (이름 없음) - 플레이어: %d/%d, Ping: %dms"), 
								i, ActualPlayerCount, MaxPlayerCount, Result.PingInMs);
							GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, SessionInfo);
						}
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
				UE_LOG(LogTemp, Warning, TEXT("========================================"));
				UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 사용 가능한 세션이 없습니다."));
				
				// Steam 세션 검색 시 추가 정보
				UE_LOG(LogTemp, Warning, TEXT("Online Subsystem: %s"), *SubsystemName);
				
				if (SubsystemName.Equals(TEXT("Steam"), ESearchCase::IgnoreCase))
				{
					UE_LOG(LogTemp, Warning, TEXT("Steam 세션을 찾지 못한 가능한 원인:"));
					UE_LOG(LogTemp, Warning, TEXT("  1. 호스트 PC에서 먼저 방 생성 → '[방 생성] CreateSession 호출 성공' 로그 확인"));
					UE_LOG(LogTemp, Warning, TEXT("  2. 두 PC가 같은 LAN(와이파이/유선)에 연결되어 있는지 확인"));
					UE_LOG(LogTemp, Warning, TEXT("  3. Steam 양쪽 실행·로그인, 방화벽에서 게임/Steam 허용"));
					UE_LOG(LogTemp, Warning, TEXT("  4. 방 찾기 버튼 한 번 더 눌러 재검색"));
				}
				UE_LOG(LogTemp, Warning, TEXT("========================================"));
				
				// 화면에 디버그 메시지 표시
				if (GEngine)
				{
					FString WarningMsg = TEXT("[방 찾기] 사용 가능한 세션이 없습니다.\n");
					WarningMsg += TEXT("1. 호스트에서 먼저 방 만들기 → 생성 성공 로그 확인\n");
					WarningMsg += TEXT("2. 두 PC 같은 LAN 연결 확인\n");
					WarningMsg += TEXT("3. 방 찾기 다시 눌러 재검색");
					GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Yellow, WarningMsg);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[방 찾기] 실패: SessionSearch가 유효하지 않습니다. (bWasSuccessful=true이지만)"));
			UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 참고: 이는 비동기 타이밍 문제일 수 있습니다."));
			
			// 화면에 디버그 메시지 표시
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 찾기] 실패: SessionSearch가 유효하지 않습니다!"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] 실패: 세션 찾기에 실패했습니다. (bWasSuccessful=false)"));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 찾기] 실패: 세션 찾기에 실패했습니다!"));
		}
	}

	OnFindSessionsComplete.Broadcast(Results);
	
	// 블루프린트용 이벤트도 브로드캐스트 (세션 개수 전달)
	OnFindSessionsCompleteBP.Broadcast(Results.Num());
}

void ABRGameSession::OnJoinSessionCompleteDelegate(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	bool bWasSuccessful = (Result == EOnJoinSessionCompleteResult::Success);
	
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Log, TEXT("[방 참가] 성공: 세션 참가 완료 - %s"), *InSessionName.ToString());
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			FString Message = FString::Printf(TEXT("[방 참가] 성공! 세션: %s"), *InSessionName.ToString());
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, Message);
		}
		
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
						
						// 화면에 디버그 메시지 표시
						if (GEngine)
						{
							FString TravelMsg = FString::Printf(TEXT("[방 참가] 서버로 이동 중: %s"), *TravelURL);
							GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TravelMsg);
						}
						
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
						
						// 화면에 디버그 메시지 표시
						if (GEngine)
						{
							GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("[방 참가] 호스트는 다른 방에 참가할 수 없습니다!"));
						}
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[방 참가] 실패: 연결 문자열을 가져올 수 없습니다."));
				
				// 화면에 디버그 메시지 표시
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 참가] 실패: 연결 문자열을 가져올 수 없습니다!"));
				}
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
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			FString ErrorMsg = FString::Printf(TEXT("[방 참가] 실패: %s"), *ErrorMessage);
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, ErrorMsg);
		}
		
		if (Result == EOnJoinSessionCompleteResult::AlreadyInSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("[방 참가] 참고: 같은 프로세스에서 생성한 방에 참가할 수 없습니다."));
			UE_LOG(LogTemp, Warning, TEXT("[방 참가] 참고: 다른 게임 인스턴스를 실행하여 테스트해보세요."));
			
			// 화면에 디버그 메시지 표시
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("다른 게임 인스턴스를 실행하여 테스트해보세요."));
			}
		}
	}

	OnJoinSessionComplete.Broadcast(bWasSuccessful);
}

int32 ABRGameSession::GetSessionCount() const
{
	if (SessionSearch.IsValid())
	{
		return SessionSearch->SearchResults.Num();
	}
	return 0;
}

FString ABRGameSession::GetSessionName(int32 SessionIndex) const
{
	if (!SessionSearch.IsValid())
	{
		return FString();
	}

	if (SessionIndex < 0 || SessionIndex >= SessionSearch->SearchResults.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GetSessionName] 잘못된 세션 인덱스: %d (범위: 0~%d)"), 
			SessionIndex, SessionSearch->SearchResults.Num() - 1);
		return FString();
	}

	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SessionIndex];
	FString FoundSessionName;
	
	if (Result.Session.SessionSettings.Get(FName(TEXT("SESSION_NAME")), FoundSessionName))
	{
		return FoundSessionName;
	}

	return FString();
}

bool ABRGameSession::HasActiveSession() const
{
	if (SessionInterface.IsValid())
	{
		auto Session = SessionInterface->GetNamedSession(NAME_GameSession);
		return Session != nullptr;
	}
	return false;
}
