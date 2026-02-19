// BRGameSession.cpp - NGDA 스타일 Steam OSS 구현
#include "BRGameSession.h"
#include "BRGameInstance.h"
#include "BRGameMode.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

ABRGameSession::ABRGameSession()
	: bIsSearchingSessions(false)
	, FindSessionsRetryCount(0)
	, PendingRoomName(TEXT(""))
	, bPendingCreateSession(false)
	, bReturnToMainMenuAfterDestroy(false)
{
}

void ABRGameSession::BeginPlay()
{
	Super::BeginPlay();
	
	// Online Subsystem 초기화
	InitializeOnlineSubsystem();
	
	// PendingRoomName이 있으면 자동 방 생성
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	UBRGameInstance* BRGI = Cast<UBRGameInstance>(World->GetGameInstance());
	if (!BRGI)
	{
		return;
	}

	// 로비 맵(Main_Scene)에서 시작한 경우 "시작한 방 제외" 플래그 해제. Stage 맵을 바로 연 경우 클라이언트가 방 목록에서 서버 방을 볼 수 있도록.
	FString LevelName = UGameplayStatics::GetCurrentLevelName(World, true);
	if (LevelName.IsEmpty())
	{
		LevelName = World->GetMapName();
		LevelName.RemoveFromStart(World->StreamingLevelsPrefix);
	}
	if (LevelName.Contains(TEXT("Main_Scene")))
	{
		BRGI->SetExcludeOwnSessionFromSearch(false);
	}
	
	FString RoomName = BRGI->GetPendingRoomName();
	if (RoomName.IsEmpty() || HasActiveSession())
	{
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[GameSession] PendingRoomName 발견: %s"), *RoomName);
	
	// Online Subsystem이 준비될 때까지 대기 후 방 생성
	// 람다에서 World를 캡처하지 않고 콜백 시점에 GetWorld()로 가져와 댕글링 포인터 크래시 방지
	// 멤버 핸들 사용: PIE 종료 시 EndPlay/UnbindSessionDelegatesForPIEExit에서 클리어해 월드 참조 잔류 방지
	FString RoomNameCopy = RoomName; // 복사본 저장
	World->GetTimerManager().SetTimer(PendingCreateRoomTimerHandle, [this, RoomNameCopy]()
	{
		if (!IsValid(this))
		{
			UE_LOG(LogTemp, Error, TEXT("[GameSession] 타이머 콜백: GameSession이 유효하지 않습니다."));
			return;
		}
		
		UWorld* CurrentWorld = GetWorld();
		if (!CurrentWorld || !IsValid(CurrentWorld))
		{
			UE_LOG(LogTemp, Error, TEXT("[GameSession] 타이머 콜백: GetWorld()가 유효하지 않습니다."));
			return;
		}
		
		// SessionInterface가 준비되었는지 확인
		if (!SessionInterface.IsValid())
		{
			InitializeOnlineSubsystem();
		}
		
		// SessionInterface가 준비되었고 세션이 없으면 방 생성
		if (SessionInterface.IsValid() && !HasActiveSession())
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameSession] 자동 방 생성 실행: %s"), *RoomNameCopy);
			CreateRoomSession(RoomNameCopy);
			if (UBRGameInstance* GI = Cast<UBRGameInstance>(CurrentWorld->GetGameInstance()))
			{
				GI->ClearPendingRoomName();
			}
		}
	}, 1.0f, false); // 1초 지연 (Online Subsystem 초기화 대기)
}

void ABRGameSession::InitializeOnlineSubsystem()
{
	if (!IsValid(this) || !GetWorld())
	{
		return;
	}

	// 방 생성 흐름(PendingRoomName 있음)일 때만 상세 로그 — 일반 시작 시 로그 최소화
	UBRGameInstance* BRGI = Cast<UBRGameInstance>(GetWorld()->GetGameInstance());
	const bool bRoomCreationFlow = BRGI && !BRGI->GetPendingRoomName().IsEmpty();
	
	// NGDA 스타일: 간단하게 Online Subsystem 가져오기
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		if (bRoomCreationFlow)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameSession] Online Subsystem: NULL (방 생성 시에만 출력)"));
		}
		return;
	}
	
	if (bRoomCreationFlow)
	{
		FString SubsystemName = OnlineSubsystem->GetSubsystemName().ToString();
		UE_LOG(LogTemp, Warning, TEXT("[GameSession] Online Subsystem: %s"), *SubsystemName);
	}
	
	// SessionInterface 가져오기
	SessionInterface = OnlineSubsystem->GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		// 콜백 바인딩 (중복 방지 - 이미 바인딩되어 있으면 스킵)
		if (!SessionInterface->OnCreateSessionCompleteDelegates.IsBoundToObject(this))
		{
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &ABRGameSession::OnCreateSessionCompleteDelegate);
			SessionInterface->OnStartSessionCompleteDelegates.AddUObject(this, &ABRGameSession::OnStartSessionCompleteDelegate);
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &ABRGameSession::OnDestroySessionCompleteDelegate);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &ABRGameSession::OnFindSessionsCompleteDelegate);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &ABRGameSession::OnJoinSessionCompleteDelegate);
			if (bRoomCreationFlow)
			{
				UE_LOG(LogTemp, Warning, TEXT("[GameSession] SessionInterface 콜백 바인딩 완료"));
			}
		}
		if (bRoomCreationFlow)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameSession] SessionInterface 초기화 완료"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[GameSession] SessionInterface 초기화 실패"));
	}
}

void ABRGameSession::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	// 콜백 해제
	if (SessionInterface.IsValid())
	{
		SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnStartSessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
		SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);

		if (bIsSearchingSessions)
		{
			SessionInterface->CancelFindSessions();
			bIsSearchingSessions = false;
		}
	}
	
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryHandle);
		World->GetTimerManager().ClearTimer(PendingCreateRoomTimerHandle);
	}
}

void ABRGameSession::UnbindSessionDelegatesForPIEExit()
{
	// SessionInterface가 GameSession(this)을 붙들고 있으면 UnrealEdEngine → OSS → SessionInterface → GameSession → World 참조 사슬로
	// PIE 월드가 GC되지 않는다. PIE 종료 시 GameInstance::Shutdown/DoPIEExitCleanup에서 먼저 호출해 이 사슬을 끊는다.
	if (SessionInterface.IsValid())
	{
		// 델리게이트를 먼저 제거해 콜백이 이 GameSession을 보지 않도록 함
		SessionInterface->OnCreateSessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnStartSessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
		SessionInterface->OnFindSessionsCompleteDelegates.RemoveAll(this);
		SessionInterface->OnJoinSessionCompleteDelegates.RemoveAll(this);

		if (bIsSearchingSessions)
		{
			SessionInterface->CancelFindSessions();
			bIsSearchingSessions = false;
		}

		// PIE 종료 시 남아 있던 세션 파괴 (Unbind 후 호출 — 콜백이 이 객체를 참조하지 않음)
		if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
		{
			SessionInterface->DestroySession(NAME_GameSession);
		}

		// 검색 및 설정 데이터 초기화 (World 참조 잔류 방지)
		SessionSearch = nullptr;
		SessionSettings = nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryHandle);
		World->GetTimerManager().ClearTimer(PendingCreateRoomTimerHandle);
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
}

FString ABRGameSession::BuildTravelURL() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FString();
	}
	
	// NGDA 스타일: 로비 맵이 지정돼 있으면 그 맵으로, 없으면 현재 맵 유지
	if (ABRGameMode* GM = World->GetAuthGameMode<ABRGameMode>())
	{
		if (!GM->LobbyMapPath.IsEmpty())
		{
			return GM->LobbyMapPath + TEXT("?listen");
		}
	}
	
	// 현재 맵 경로로 리슨 URL 구성 (BRCheatManager와 동일한 방식)
	FString MapPath = UGameplayStatics::GetCurrentLevelName(World, true);
	if (MapPath.IsEmpty())
	{
		MapPath = World->GetMapName();
		MapPath.RemoveFromStart(World->StreamingLevelsPrefix);
	}
	
	// 맵 경로가 비어있으면 기본값 사용
	if (MapPath.IsEmpty())
	{
		MapPath = TEXT("/Game/Main/Level/Main_Scene");
	}
	
	// /Game/.../MapName 형식으로 변환 (이미 올바른 형식이면 그대로 사용)
	if (!MapPath.Contains(TEXT("/")))
	{
		// 짧은 이름만 있으면 전체 경로로 변환
		MapPath = FString::Printf(TEXT("/Game/Main/Level/%s"), *MapPath);
	}
	
	// .MapName 형식이 아니면 추가
	if (!MapPath.Contains(TEXT(".")))
	{
		FString BaseName = FPaths::GetBaseFilename(MapPath);
		MapPath = FString::Printf(TEXT("%s.%s"), *MapPath, *BaseName);
	}
	
	return MapPath + TEXT("?listen");
}

void ABRGameSession::CreateRoomSession(const FString& RoomName)
{
	UE_LOG(LogTemp, Warning, TEXT("[방 생성] 시작: %s"), *RoomName);
	
	if (!SessionInterface.IsValid())
	{
		InitializeOnlineSubsystem();
		if (!SessionInterface.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[방 생성] SessionInterface 초기화 실패"));
			OnCreateSessionComplete.Broadcast(false);
			return;
		}
	}
	
	UWorld* World = GetWorld();
	if (!World)
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}
	
	// Standalone 모드에서는 CreateSession 전에 ListenServer로 전환 필요
	ENetMode NetMode = World->GetNetMode();
	UE_LOG(LogTemp, Warning, TEXT("[방 생성] 현재 NetMode: %s"), 
		NetMode == NM_Standalone ? TEXT("Standalone") :
		NetMode == NM_ListenServer ? TEXT("ListenServer") :
		NetMode == NM_Client ? TEXT("Client") :
		NetMode == NM_DedicatedServer ? TEXT("DedicatedServer") : TEXT("Unknown"));
	
	if (NetMode == NM_Standalone)
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] ⚠️ Standalone 모드 감지 - ListenServer로 전환 후 세션 생성"));
		
		// 현재 맵 경로 가져오기
		FString CurrentMapPath = UGameplayStatics::GetCurrentLevelName(World, true);
		if (CurrentMapPath.IsEmpty())
		{
			CurrentMapPath = World->GetMapName();
			CurrentMapPath.RemoveFromStart(World->StreamingLevelsPrefix);
		}
		
		// 맵 경로를 /Game/.../MapName.MapName 형식으로 변환
		if (!CurrentMapPath.Contains(TEXT("/")))
		{
			CurrentMapPath = FString::Printf(TEXT("/Game/Main/Level/%s.%s"), *CurrentMapPath, *CurrentMapPath);
		}
		else if (!CurrentMapPath.Contains(TEXT(".")))
		{
			FString MapName = FPaths::GetBaseFilename(CurrentMapPath);
			CurrentMapPath = FString::Printf(TEXT("%s.%s"), *CurrentMapPath, *MapName);
		}
		
		if (!CurrentMapPath.IsEmpty())
		{
			FString ListenURL = FString::Printf(TEXT("%s?listen"), *CurrentMapPath);
			FString OpenCommand = FString::Printf(TEXT("open %s"), *ListenURL);
			
			// PendingRoomName 저장 (ListenServer 전환 후 자동 방 생성용)
			if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(World->GetGameInstance()))
			{
				BRGI->SetPendingRoomName(RoomName);
			}
			
			// ListenServer로 전환
			if (GEngine)
			{
				bool bExecResult = GEngine->Exec(World, *OpenCommand);
				UE_LOG(LogTemp, Warning, TEXT("[방 생성] Standalone → ListenServer 전환: %s, 명령: %s"), 
					bExecResult ? TEXT("성공") : TEXT("실패"), *OpenCommand);
			}
			
			// ListenServer 전환 후 자동으로 방 생성됨 (OnStart에서 PendingRoomName 처리)
			return;
		}
	}
	
	// 기존 세션이 있으면 제거
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] 기존 세션 제거 중..."));
		PendingRoomName = RoomName;
		bPendingCreateSession = true;
		SessionInterface->DestroySession(NAME_GameSession);
		return;
	}
	
	// NGDA 스타일: 세션 설정 생성
	SessionSettings = MakeShareable(new FOnlineSessionSettings());
	
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	FString SubsystemName = OnlineSubsystem ? OnlineSubsystem->GetSubsystemName().ToString() : TEXT("NULL");
	
	// NGDA 스타일: 세션 설정
	SessionSettings->bIsLANMatch = (SubsystemName == TEXT("NULL"));
	SessionSettings->bUsesPresence = true;
	SessionSettings->bShouldAdvertise = true;
	SessionSettings->bUseLobbiesIfAvailable = true;
	SessionSettings->bAllowJoinViaPresence = true;
	SessionSettings->bAllowJoinInProgress = true;

	// NGDA 스타일: CreateSession 성공 후 StartSession → ServerTravel(TravelURL)용 URL
	TravelURL = BuildTravelURL();

	// 플레이어 수 설정
	int32 MaxPlayerCount = 8;
	if (ABRGameMode* BRGM = World->GetAuthGameMode<ABRGameMode>())
	{
		MaxPlayerCount = BRGM->MaxPlayers;
	}
	SessionSettings->NumPublicConnections = MaxPlayerCount;
	
	// 세션 이름 설정
	FString SessionNameStr = RoomName.IsEmpty() ? TEXT("이름 없는 방") : RoomName;
	SessionSettings->Set(FName(TEXT("SESSION_NAME")), SessionNameStr, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	// 방 생성 시 서버장(호스트)이 이미 있으므로 현재 인원 1로 시작 (방 찾기에서 0이 아닌 1부터 표시)
	SessionSettings->Set(FName(TEXT("CURRENT_PLAYER_COUNT")), 1, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	// NGDA 스타일: CreateSession 호출 (NetMode 체크 없음 - Steam OSS가 자동으로 ListenServer 처리)
	int32 LocalUserNum = 0;
	bool bCreateResult = SessionInterface->CreateSession(LocalUserNum, NAME_GameSession, *SessionSettings);
	
	if (!bCreateResult)
	{
		UE_LOG(LogTemp, Error, TEXT("[방 생성] CreateSession 호출 실패"));
		OnCreateSessionComplete.Broadcast(false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] CreateSession 호출 성공 (비동기 처리 중)"));
	}
}

void ABRGameSession::FindSessions()
{
	UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 시작"));
	
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] SessionInterface가 유효하지 않습니다."));
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>());
		OnFindSessionsCompleteBP.Broadcast(0);
		return;
	}
	
	// 이미 검색 중이면 무시
	if (bIsSearchingSessions)
	{
		return;
	}
	
	FindSessionsInternal(false);
}

void ABRGameSession::FindSessionsRetryCallback()
{
	FindSessionsInternal(true);
}

void ABRGameSession::FindSessionsInternal(bool bIsRetry)
{
	if (!SessionInterface.IsValid())
	{
		return;
	}
	
	if (!bIsRetry)
	{
		FindSessionsRetryCount = 0;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FindSessionsRetryHandle);
		}
	}
	
	bIsSearchingSessions = true;
	
	// 방 찾기 시점 NetMode 로그 (한번 호스트였던 경우 Standalone인지 확인용)
	if (UWorld* World = GetWorld())
	{
		ENetMode NetMode = World->GetNetMode();
		UE_LOG(LogTemp, Log, TEXT("[방 찾기] 검색 시작 - NetMode=%s"),
			NetMode == NM_Standalone ? TEXT("Standalone") :
			NetMode == NM_ListenServer ? TEXT("ListenServer") :
			NetMode == NM_Client ? TEXT("Client") : TEXT("Other"));
	}

	// 이전 검색 취소
	SessionInterface->CancelFindSessions();
	
	// 세션 검색 설정
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);
	
	// LAN 여부 설정
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem && OnlineSubsystem->GetSubsystemName() == "NULL")
	{
		SessionSearch->bIsLanQuery = true;
	}
	else
	{
		SessionSearch->bIsLanQuery = false;
	}
	
	// FindSessions 호출
	bool bFindSessionsResult = SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	if (!bFindSessionsResult)
	{
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] FindSessions 호출 실패"));
		bIsSearchingSessions = false;
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>());
		OnFindSessionsCompleteBP.Broadcast(0);
	}
}

void ABRGameSession::JoinSessionByIndex(int32 SessionIndex)
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] SessionInterface가 유효하지 않습니다."));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}
	
	if (!SessionSearch.IsValid() || SessionIndex < 0 || SessionIndex >= SessionSearch->SearchResults.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 잘못된 세션 인덱스: %d"), SessionIndex);
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
		UE_LOG(LogTemp, Error, TEXT("[방 참가] SessionInterface가 유효하지 않습니다."));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}
	
	// 기존 세션이 있으면 제거
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		SessionInterface->DestroySession(NAME_GameSession);
	}
	
	// JoinSession 호출
	SessionInterface->JoinSession(0, NAME_GameSession, SessionResult);
}

void ABRGameSession::OnCreateSessionCompleteDelegate(FName InSessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] 성공! StartSession 후 ServerTravel 예정..."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
				TEXT("[방 생성] 성공! 리슨 서버로 전환 중..."));
		}
		// NGDA 스타일: CreateSession 성공 후 반드시 StartSession 호출 → OnStartSessionComplete에서 ServerTravel
		if (SessionInterface.IsValid())
		{
			auto Session = SessionInterface->GetNamedSession(InSessionName);
			if (Session && (Session->SessionState == EOnlineSessionState::Starting || Session->SessionState == EOnlineSessionState::InProgress))
			{
				UE_LOG(LogTemp, Warning, TEXT("[방 생성] 세션이 이미 시작 중이거나 진행 중입니다 (State: %s). OnStartSessionCompleteDelegate를 직접 호출합니다."), EOnlineSessionState::ToString(Session->SessionState));
				OnStartSessionCompleteDelegate(InSessionName, true);
			}
			else
			{
				SessionInterface->StartSession(InSessionName);
			}
		}
		else
		{
			OnCreateSessionComplete.Broadcast(false);
			return;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[방 생성] 실패"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 생성] 실패"));
		}
		OnCreateSessionComplete.Broadcast(false);
	}
}

void ABRGameSession::OnStartSessionCompleteDelegate(FName InSessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 생성] StartSession 성공. ServerTravel: %s"), *TravelURL);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
				FString::Printf(TEXT("세션 시작 완료! 맵 이동: %s"), *TravelURL));
		}
		UWorld* World = GetWorld();
		if (World && !TravelURL.IsEmpty())
		{
			// PIE(Play In Editor) 환경 감지
			bool bIsPIE = World->IsPlayInEditor();
			
			if (bIsPIE)
			{
				// PIE에서는 ServerTravel을 스킵 (크래시 방지)
				// PIE에서는 현재 맵에서 세션만 활성화
				UE_LOG(LogTemp, Warning, TEXT("[방 생성] PIE 모드 감지 - ServerTravel 스킵 (현재 맵에서 세션 활성화)"));
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
						TEXT("PIE 모드: 현재 맵에서 세션이 활성화되었습니다."));
				}
			}
			else
			{
				// 일반 실행 환경에서는 ServerTravel 실행
				World->ServerTravel(TravelURL, true);
				UE_LOG(LogTemp, Warning, TEXT("[방 생성] ServerTravel 호출 완료. 맵 재로드 후 ListenServer 모드로 전환됩니다."));
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
						TEXT("맵 재로드 중... 리슨 서버로 전환됩니다."));
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[방 생성] ServerTravel 스킵: World 또는 TravelURL 없음"));
		}
		OnCreateSessionComplete.Broadcast(true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[방 생성] StartSession 실패"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[방 생성] StartSession 실패"));
		}
		OnCreateSessionComplete.Broadcast(false);
	}
}

void ABRGameSession::OnDestroySessionCompleteDelegate(FName InSessionName, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("[방 생성] 세션 제거 완료: %s"), bWasSuccessful ? TEXT("성공") : TEXT("실패"));

	// 방 나가기(호스트): 메인 맵으로 이동 (listen 없이 이동해 다른 방 참가 가능하도록 함)
	if (bReturnToMainMenuAfterDestroy)
	{
		bReturnToMainMenuAfterDestroy = false;
		UWorld* World = GetWorld();
		if (World)
		{
			// 프로젝트 설정의 Game Default Map과 동일한 경로 사용. ?listen 제거 → 이 인스턴스는 더 이상 리슨하지 않음.
			// ServerTravel에는 패키지 경로만 사용 (.MapName 접미사 사용 시 CanServerTravel에서 LongPackageNames 규칙으로 차단됨)
			FString DefaultMap = TEXT("/Game/Main/Level/Main_Scene");
			World->ServerTravel(DefaultMap, true);
			UE_LOG(LogTemp, Log, TEXT("[방 나가기] 호스트: 메인 맵으로 이동 (listen 해제): %s"), *DefaultMap);
		}
		return;
	}
	
	if (bPendingCreateSession && !PendingRoomName.IsEmpty())
	{
		bPendingCreateSession = false;
		FString RoomName = PendingRoomName;
		PendingRoomName = TEXT("");
		CreateRoomSession(RoomName);
	}
}

void ABRGameSession::OnFindSessionsCompleteDelegate(bool bWasSuccessful)
{
	bIsSearchingSessions = false;
	
	TArray<FOnlineSessionSearchResult> Results;
	
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		Results = SessionSearch->SearchResults;

		// 시작한 방 제외: 호스트이고, WBP_Start로 게임맵에 들어간 뒤일 때만 적용 (로비에 있을 때는 PIE 클라이언트가 방 목록에 보이도록)
		UWorld* World = GetWorld();
		const bool bAmHost = World && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer);
		UBRGameInstance* BRGI = World ? Cast<UBRGameInstance>(World->GetGameInstance()) : nullptr;
		const bool bExcludeOwn = bAmHost && BRGI && BRGI->GetExcludeOwnSessionFromSearch();
		if (bExcludeOwn)
		{
			const FNamedOnlineSession* LocalSession = SessionInterface->GetNamedSession(NAME_GameSession);
			if (LocalSession)
			{
				FString LocalSessionId = LocalSession->GetSessionIdStr();
				int32 Removed = 0;
				for (int32 i = SessionSearch->SearchResults.Num() - 1; i >= 0; --i)
				{
					if (SessionSearch->SearchResults[i].Session.GetSessionIdStr() == LocalSessionId)
					{
						SessionSearch->SearchResults.RemoveAt(i);
						Removed++;
					}
				}
				if (Removed > 0)
				{
					UE_LOG(LogTemp, Log, TEXT("[방 찾기] 자신이 호스팅 중인 세션 %d개를 검색 결과에서 제외 (게임맵 이동 후)"), Removed);
				}
				Results = SessionSearch->SearchResults;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 완료: %d개 세션 발견"), Results.Num());
		
		// 0건일 때 Steam/Null 모두 최대 2회 자동 재검색 (한번 호스트였던 경우 OSS 지연 대응)
		if (Results.Num() == 0)
		{
			IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
			FString SubsystemName = OnlineSubsystem ? OnlineSubsystem->GetSubsystemName().ToString() : TEXT("NULL");
			
			const bool bRetryAllowed = (SubsystemName.Equals(TEXT("Steam"), ESearchCase::IgnoreCase) || SubsystemName.Equals(TEXT("NULL"), ESearchCase::IgnoreCase))
				&& FindSessionsRetryCount < MaxFindSessionsRetries;
			if (bRetryAllowed)
			{
				FindSessionsRetryCount++;
				UE_LOG(LogTemp, Warning, TEXT("[방 찾기] 세션 0건. 2초 후 재검색 (%d/%d) [OSS=%s]"), FindSessionsRetryCount, MaxFindSessionsRetries, *SubsystemName);
				if (World)
				{
					World->GetTimerManager().SetTimer(FindSessionsRetryHandle, this, &ABRGameSession::FindSessionsRetryCallback, 2.0f, false);
				}
				return;
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[방 찾기] 실패"));
	}
	
	OnFindSessionsComplete.Broadcast(Results);
	OnFindSessionsCompleteBP.Broadcast(Results.Num());
}

void ABRGameSession::OnJoinSessionCompleteDelegate(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	bool bWasSuccessful = (Result == EOnJoinSessionCompleteResult::Success);
	
	if (!SessionInterface.IsValid())
	{
		OnJoinSessionComplete.Broadcast(false);
		return;
	}
	
	// 연결 문자열 가져오기
	FString ConnectURL;
	if (!SessionInterface->GetResolvedConnectString(InSessionName, ConnectURL))
	{
		UE_LOG(LogTemp, Error, TEXT("[방 참가] 연결 주소를 가져올 수 없습니다."));
		OnJoinSessionComplete.Broadcast(false);
		return;
	}
	
	// ClientTravel 호출
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			UE_LOG(LogTemp, Warning, TEXT("[방 참가] 서버로 이동: %s"), *ConnectURL);
			PC->ClientTravel(ConnectURL, ETravelType::TRAVEL_Absolute);
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
	if (!SessionSearch.IsValid() || SessionIndex < 0 || SessionIndex >= SessionSearch->SearchResults.Num())
	{
		return FString();
	}
	
	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SessionIndex];
	FString FoundSessionName;
	Result.Session.SessionSettings.Get(FName(TEXT("SESSION_NAME")), FoundSessionName);
	
	if (!FoundSessionName.IsEmpty())
	{
		return FoundSessionName;
	}
	return TEXT("(이름 없음)");
}

int32 ABRGameSession::GetSessionMaxPlayers(int32 SessionIndex) const
{
	if (!SessionSearch.IsValid() || SessionIndex < 0 || SessionIndex >= SessionSearch->SearchResults.Num())
	{
		return 0;
	}
	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SessionIndex];
	return Result.Session.SessionSettings.NumPublicConnections;
}

int32 ABRGameSession::GetSessionCurrentPlayers(int32 SessionIndex) const
{
	if (!SessionSearch.IsValid() || SessionIndex < 0 || SessionIndex >= SessionSearch->SearchResults.Num())
	{
		return 0;
	}
	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SessionIndex];
	// 호스트가 UpdateSessionPlayerCount로 설정한 현재 인원을 우선 사용 (일부 OSS는 NumOpenPublicConnections를 갱신하지 않아 0으로 나올 수 있음)
	int32 AdvertisedCount = 0;
	if (Result.Session.SessionSettings.Get(FName(TEXT("CURRENT_PLAYER_COUNT")), AdvertisedCount) && AdvertisedCount >= 0)
	{
		int32 Max = Result.Session.SessionSettings.NumPublicConnections;
		return FMath::Clamp(AdvertisedCount, 0, Max);
	}
	// 폴백: 최대 - 빈 슬롯
	int32 Max = Result.Session.SessionSettings.NumPublicConnections;
	int32 Open = Result.Session.NumOpenPublicConnections;
	return FMath::Max(0, Max - Open);
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

void ABRGameSession::UpdateSessionPlayerCount(int32 PlayerCount)
{
	if (!SessionInterface.IsValid() || !SessionSettings.IsValid())
	{
		return;
	}
	if (SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		return;
	}
	SessionSettings->Set(FName(TEXT("CURRENT_PLAYER_COUNT")), PlayerCount, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	SessionInterface->UpdateSession(NAME_GameSession, *SessionSettings, true);
}

void ABRGameSession::DestroySessionAndReturnToMainMenu()
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[방 나가기] SessionInterface가 없습니다."));
		return;
	}
	if (SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[방 나가기] 활성 세션이 없습니다."));
		return;
	}
	bReturnToMainMenuAfterDestroy = true;
	SessionInterface->DestroySession(NAME_GameSession);
	UE_LOG(LogTemp, Log, TEXT("[방 나가기] 호스트: 세션 종료 요청 (완료 시 메인 맵으로 이동)"));
}
