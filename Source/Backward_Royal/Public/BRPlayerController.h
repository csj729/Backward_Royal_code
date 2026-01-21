// BRPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BRPlayerController.generated.h"

UCLASS()
class BACKWARD_ROYAL_API ABRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABRPlayerController();

	// 방 생성
	UFUNCTION(BlueprintCallable, Exec, Category = "Session")
	void CreateRoom(const FString& RoomName = TEXT("TestRoom"));

	// 방 찾기
	UFUNCTION(BlueprintCallable, Exec, Category = "Session")
	void FindRooms();

	// 방 참가
	UFUNCTION(BlueprintCallable, Exec, Category = "Session")
	void JoinRoom(int32 SessionIndex);

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

	// 현재 상태 확인: ShowRoomInfo
	UFUNCTION(Exec, Category = "Room")
	void ShowRoomInfo();

	// 클라이언트 RPC 함수들
	UFUNCTION(Client, Reliable)
	void ClientNotifyGameStarting();

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

	// UI 전환 함수들
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowEntranceMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowJoinMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowLobbyMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideCurrentMenu();

	// 현재 표시 중인 위젯 가져오기
	UFUNCTION(BlueprintCallable, Category = "UI")
	class UUserWidget* GetCurrentMenuWidget() const { return CurrentMenuWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	// 네트워크 연결 실패 감지
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

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

	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();

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
};

