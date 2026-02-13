// BRPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "BRPlayerController.generated.h"

// 전방 선언
class UNetDriver;
class UNetConnection;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPawnChanged, APawn*, NewPawn);

UCLASS()
class BACKWARD_ROYAL_API ABRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABRPlayerController();

	// 방 생성
	UFUNCTION(BlueprintCallable, Exec, Category = "Session")
	void CreateRoom(const FString& RoomName = TEXT("TestRoom"));

	// 방 생성 및 플레이어 이름 설정
	UFUNCTION(BlueprintCallable, Category = "Session")
	void CreateRoomWithPlayerName(const FString& RoomName, const FString& PlayerName);

	// 방 찾기
	UFUNCTION(BlueprintCallable, Exec, Category = "Session")
	void FindRooms();

	// 방 참가
	UFUNCTION(BlueprintCallable, Exec, Category = "Session")
	void JoinRoom(int32 SessionIndex);

	// 방 참가 및 플레이어 이름 설정
	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinRoomWithPlayerName(int32 SessionIndex, const FString& PlayerName);

	/** 로비에서 방 나가기. 클라이언트는 서버 연결을 끊고, 호스트는 세션을 종료한 뒤 메인 맵으로 이동합니다. */
	UFUNCTION(BlueprintCallable, Category = "Session")
	void LeaveRoom();

	// 준비 상태 토글
	UFUNCTION(BlueprintCallable, Exec, Category = "Room")
	void ToggleReady();

	// 랜덤 팀 배정 요청 (방장만)
	UFUNCTION(BlueprintCallable, Exec, Category = "Team")
	void RandomTeams();

	// 플레이어 팀 변경 요청 (방장만)
	UFUNCTION(BlueprintCallable, Exec, Category = "Team")
	void ChangeTeam(int32 PlayerIndex, int32 TeamNumber);

	// 게임 시작 요청 (방장만)
	UFUNCTION(BlueprintCallable, Exec, Category = "Game")
	void StartGame();

	// 플레이어 이름 설정
	UFUNCTION(BlueprintCallable, Category = "User Info")
	void SetPlayerName(const FString& NewPlayerName);

	// 팀 번호 설정 (자신의 팀 번호)
	UFUNCTION(BlueprintCallable, Category = "Team")
	void SetMyTeamNumber(int32 NewTeamNumber);

	// 플레이어 역할 설정 (하체/상체)
	UFUNCTION(BlueprintCallable, Category = "Player Role")
	void SetMyPlayerRole(bool bLowerBody);

	/** 로비: Entry → SelectTeam 슬롯 배치 요청. TeamIndex 0~3=팀1~4, SlotIndex 0=관전 1=1Player(하체) 2=2Player(상체) */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestAssignToLobbyTeam(int32 TeamIndex, int32 SlotIndex);

	/** 로비: SelectTeam 슬롯 → Entry로 이동 요청 (해당 팀/슬롯의 플레이어를 Entry 첫 빈 자리로) */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestMoveToLobbyEntry(int32 TeamIndex, int32 SlotIndex);

	/** 로비: 자신이 현재 있는 팀 슬롯을 찾아 대기열(Entry)로 이동 요청. 팀에 없으면 아무 동작 안 함 */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestMoveMyPlayerToLobbyEntry();

	// 현재 상태 확인: ShowRoomInfo
	UFUNCTION(Exec, Category = "Room")
	void ShowRoomInfo();

	// 클라이언트 RPC 함수들
	UFUNCTION(Client, Reliable)
	void ClientNotifyGameStarting();

	/** PIE 등에서 ServerTravel이 클라이언트를 따라오지 않을 때, 서버가 지정한 URL로 직접 이동 */
	UFUNCTION(Client, Reliable)
	void ClientTravelToGameMap(const FString& TravelURL);

	/** 입장 직후 방 제목 전달 (복제 대기 없이 "○○'s Game" 즉시 표시) */
	UFUNCTION(Client, Reliable)
	void ClientReceiveRoomTitle(const FString& RoomTitle);

	/** 서버가 입장 직후 클라이언트에 로비 UI 갱신 요청 (늦게 들어온 3·4번째 클라이언트 UI 동기화용) */
	UFUNCTION(Client, Reliable)
	void ClientRequestLobbyUIRefresh();

	// 역할에 따른 입력 매핑 교체 함수
	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetupRoleInput(bool bIsLower);

	// 에디터에서 할당할 수 있도록 Mapping Context 변수 추가
	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* LowerBodyContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	class UInputMappingContext* UpperBodyContext;

	// ========== UI 관리 시스템 ==========
	
	// 초기 UI 위젯 클래스 (에디터에서 설정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> EntranceMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> JoinMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> LobbyMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> MainScreenWidgetClass;

	// WBP_MainScreen 설정 (블루프린트에서 호출)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetMainScreenWidget(class UUserWidget* Widget);

	// UI 전환 함수들
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowEntranceMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowJoinMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowLobbyMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowMainScreen();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideMainScreen();

	// WBP_MainScreen의 WidgetSwitcher 인덱스를 EntranceMenu로 설정
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetMainScreenToEntranceMenu();

	// WBP_MainScreen의 WidgetSwitcher 인덱스를 LobbyMenu로 설정
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetMainScreenToLobbyMenu();

	/** PIE/서버 종료 시 위젯 참조를 먼저 끊을 때 호출 (GameInstance DoPIEExitCleanup에서 호출) */
	void ClearUIForShutdown();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideCurrentMenu();

	// 현재 표시 중인 위젯 가져오기
	UFUNCTION(BlueprintCallable, Category = "UI")
	class UUserWidget* GetCurrentMenuWidget() const { return CurrentMenuWidget; }

	// 위젯이 이 이벤트 감지
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPawnChanged OnPawnChanged;

	// 로컬 GameInstance의 커마 정보를 서버로 전송하는 함수
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SubmitCustomizationToServer();

	// 관전 모드로 전환 (서버에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Spectating")
	void StartSpectatingMode();

	// [클라이언트] 관전 모드 진입 시 UI 처리 요청
	UFUNCTION(Client, Reliable)
	void ClientHandleSpectatorUI();

	// [BP 구현] 관전 모드 진입 시 UI 변경 (HUD 숨기기 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Spectating")
	void OnEnterSpectatorMode();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 서버에서 빙의했을 때 호출됨
	virtual void OnPossess(APawn* aPawn) override;

	// 클라이언트에서 폰 정보가 복제되었을 때 호출됨
	virtual void OnRep_Pawn() override;

	/** 상체 빙의 시 ViewTarget/입력 적용 (복제 타이밍에 따라 한 클라이언트만 누락되는 현상 방지용으로 다음 틱에도 재적용) */
	void ApplyUpperBodyViewAndInput();

	// 네트워크 연결 실패 감지
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	// 방 생성 완료 핸들러
	UFUNCTION()
	void HandleCreateRoomComplete(bool bWasSuccessful);

	// 서버 RPC 함수들
	UFUNCTION(Server, Reliable)
	void ServerCreateRoom(const FString& RoomName);

	UFUNCTION(Server, Reliable)
	void ServerFindRooms();

	UFUNCTION(Server, Reliable)
	void ServerJoinRoom(int32 SessionIndex);

	UFUNCTION(Server, Reliable)
	void ServerToggleReady();

	UFUNCTION(Server, Reliable)
	void ServerRequestRandomTeams();

	UFUNCTION(Server, Reliable)
	void ServerRequestChangePlayerTeam(int32 PlayerIndex, int32 NewTeamNumber);

	/** 게임 시작 요청 (클라이언트→서버). 주의: 블루프린트에서 오버라이드 시 파라미터를 추가/변경하면 ReceivePropertiesForRPC Mismatch로 연결이 끊깁니다. 시그니처 변경 금지. */
	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();

	UFUNCTION(Server, Reliable)
	void ServerSetPlayerName(const FString& NewPlayerName);

	UFUNCTION(Server, Reliable)
	void ServerSetTeamNumber(int32 NewTeamNumber);

	UFUNCTION(Server, Reliable)
	void ServerSetPlayerRole(bool bLowerBody);

	UFUNCTION(Server, Reliable)
	void ServerRequestAssignToLobbyTeam(int32 TeamIndex, int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerRequestMoveToLobbyEntry(int32 TeamIndex, int32 SlotIndex);

private:
	// 내부 헬퍼 함수들
	void RequestRandomTeams();
	void RequestChangePlayerTeam(int32 PlayerIndex, int32 NewTeamNumber);
	void RequestStartGame();

	// UI 관리 내부 함수
	void ShowMenuWidget(TSubclassOf<class UUserWidget> WidgetClass);
	
	// 현재 표시 중인 위젯
	UPROPERTY()
	class UUserWidget* CurrentMenuWidget;

	// WBP_MainScreen 추적 (블루프린트에서 설정 가능)
	UPROPERTY()
	class UUserWidget* MainScreenWidget;

	// BeginPlay UI 초기화 타이머 (EndPlay에서 해제하여 open ?listen 크래시 방지)
	FTimerHandle BeginPlayUITimerHandle;

	/** 상체 ViewTarget/입력 지연 재적용 타이머 (OnRep_Pawn 후 한 틱 뒤 재적용) */
	FTimerHandle UpperBodyViewInputDelayHandle;

	FTimerHandle TimerHandle_RetrySubmitCustomization;

	/** 호스트가 방 나가기 후 메인 맵에서 ListenServer NetDriver를 한 번만 종료해 Standalone으로 전환 (방 찾기 가능하도록) */
	FTimerHandle ShutdownListenServerTimerHandle;
	void TryShutdownListenServerForRoomSearch();
};

