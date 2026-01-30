// BRWidgetFunctionLibrary.h
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BRUserInfo.h"
#include "BRWidgetFunctionLibrary.generated.h"

class ABRPlayerController;
class ABRGameSession;
class ABRGameState;
class UBRGameInstance;
class ABRPlayerState;
class UUserWidget;
class UVerticalBox;
class UScrollBox;
class UWidget;

/**
 * 블루프린트 위젯에서 서버 함수를 호출하기 위한 Function Library
 * 상속 변경 없이 블루프린트에서 사용 가능
 */
UCLASS()
class BACKWARD_ROYAL_API UBRWidgetFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================
	// PlayerController 관련 함수들
	// ============================================
	
	// PlayerController 가져오기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|PlayerController", meta = (WorldContext = "WorldContextObject", CallInEditor = "true"))
	static ABRPlayerController* GetBRPlayerController(const UObject* WorldContextObject);
	
	// PlayerController 가져오기 (안전한 버전 - 유효성 검사 포함)
	UFUNCTION(BlueprintCallable, Category = "BR Widget|PlayerController", meta = (WorldContext = "WorldContextObject", CallInEditor = "true"))
	static bool GetBRPlayerControllerSafe(const UObject* WorldContextObject, ABRPlayerController*& OutPlayerController);

	// 방 생성
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject"))
	static void CreateRoom(const UObject* WorldContextObject, const FString& RoomName);

	/** 방 생성 + 플레이어 이름 설정. EntranceMenu의 EditableText 값을 PlayerName에 넣으면 로비/PlayerState에 반영됨 */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject", DisplayName = "Create Room With Player Name"))
	static void CreateRoomWithPlayerName(const UObject* WorldContextObject, const FString& RoomName, const FString& PlayerName);

	// 방 찾기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject"))
	static void FindRooms(const UObject* WorldContextObject);

	// 방 참가
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject"))
	static void JoinRoom(const UObject* WorldContextObject, int32 SessionIndex);

	/** 로비에서 방 나가기. 클라이언트는 서버 연결을 끊고, 호스트는 세션 종료 후 메인 맵으로 이동합니다. */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject"))
	static void LeaveRoom(const UObject* WorldContextObject);

	/** LAN 전용(true) / 인터넷 매칭(false). 방 만들기·방 찾기 전에 설정. */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject"))
	static void SetUseLANOnly(const UObject* WorldContextObject, bool bLAN);

	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject"))
	static bool GetUseLANOnly(const UObject* WorldContextObject);

	// 준비 상태 토글
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Room", meta = (WorldContext = "WorldContextObject"))
	static void ToggleReady(const UObject* WorldContextObject);

	// 랜덤 팀 배정 (방장만)
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Team", meta = (WorldContext = "WorldContextObject"))
	static void RandomTeams(const UObject* WorldContextObject);

	// 플레이어 팀 변경 (방장만)
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Team", meta = (WorldContext = "WorldContextObject"))
	static void ChangeTeam(const UObject* WorldContextObject, int32 PlayerIndex, int32 TeamNumber);

	/** 로비: 자신을 SelectTeam 슬롯에 배치 요청. TeamIndex 0~3=팀1~4, SlotIndex 0=1Player 1=2Player */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Lobby", meta = (WorldContext = "WorldContextObject"))
	static void RequestAssignToLobbyTeam(const UObject* WorldContextObject, int32 TeamIndex, int32 SlotIndex);

	/** 로비: SelectTeam 슬롯의 플레이어를 Entry로 이동 요청 */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Lobby", meta = (WorldContext = "WorldContextObject"))
	static void RequestMoveToLobbyEntry(const UObject* WorldContextObject, int32 TeamIndex, int32 SlotIndex);

	// 게임 시작 (방장만)
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Game", meta = (WorldContext = "WorldContextObject"))
	static void StartGame(const UObject* WorldContextObject);

	// ============================================
	// GameSession 관련 함수들
	// ============================================
	
	// GameSession 가져오기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|GameSession", meta = (WorldContext = "WorldContextObject"))
	static ABRGameSession* GetBRGameSession(const UObject* WorldContextObject);

	// GameInstance 가져오기 (Cast 불필요, 블루프린트에서 바로 사용)
	UFUNCTION(BlueprintCallable, Category = "BR Widget|GameInstance", meta = (WorldContext = "WorldContextObject", DisplayName = "Get BR Game Instance"))
	static class UBRGameInstance* GetBRGameInstance(const UObject* WorldContextObject);

	/** 플레이어 이름 저장 (Join Menu로 넘어가기 전에 호출. Get Game Instance + Cast 없이 사용) */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|GameInstance", meta = (WorldContext = "WorldContextObject", DisplayName = "Set Player Name"))
	static void SetPlayerName(const UObject* WorldContextObject, const FString& PlayerName);

	// ============================================
	// GameState 관련 함수들
	// ============================================
	
	// GameState 가져오기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|GameState", meta = (WorldContext = "WorldContextObject"))
	static ABRGameState* GetBRGameState(const UObject* WorldContextObject);

	/** 로비 방 제목 "○○'s Game" 표시용. 캐시(RPC) 우선, 없으면 GameState (입장 직후 즉시 표시) */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|GameState", meta = (WorldContext = "WorldContextObject"))
	static FString GetRoomTitleForDisplay(const UObject* WorldContextObject);

	/** 로비 플레이어 이름 표시용. PlayerName 사용, UserUID는 절대 반환하지 않음. 빈 문자열/UID와 같으면 "Player N". */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|GameState", meta = (DisplayName = "Get Display Name For Lobby"))
	static FString GetDisplayNameForLobby(const FBRUserInfo& UserInfo);

	// ============================================
	// PlayerState 관련 함수들
	// ============================================
	
	// PlayerState 가져오기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|PlayerState", meta = (WorldContext = "WorldContextObject"))
	static ABRPlayerState* GetBRPlayerState(const UObject* WorldContextObject);

	// 방장 여부 확인
	UFUNCTION(BlueprintCallable, Category = "BR Widget|PlayerState", meta = (WorldContext = "WorldContextObject"))
	static bool IsHost(const UObject* WorldContextObject);

	// 준비 상태 확인
	UFUNCTION(BlueprintCallable, Category = "BR Widget|PlayerState", meta = (WorldContext = "WorldContextObject"))
	static bool IsReady(const UObject* WorldContextObject);

	// ============================================
	// 네트워크 연결 상태 확인
	// ============================================
	
	/** 클라이언트가 서버에 연결되었는지 확인 (Standalone 모드에서도 사용 가능) */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Network", meta = (WorldContext = "WorldContextObject"))
	static bool IsConnectedToServer(const UObject* WorldContextObject);

	/** 현재 네트워크 모드 확인 (Standalone/Client/ListenServer 등) */
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Network", meta = (WorldContext = "WorldContextObject"))
	static FString GetNetworkMode(const UObject* WorldContextObject);

	// ============================================
	// UI 관련 함수들
	// ============================================
	
	// 메인 스크린 표시
	UFUNCTION(BlueprintCallable, Category = "BR Widget|UI", meta = (WorldContext = "WorldContextObject"))
	static void ShowMainScreen(const UObject* WorldContextObject);

	// 입장 메뉴 표시
	UFUNCTION(BlueprintCallable, Category = "BR Widget|UI", meta = (WorldContext = "WorldContextObject"))
	static void ShowEntranceMenu(const UObject* WorldContextObject);

	// 참가 메뉴 표시
	UFUNCTION(BlueprintCallable, Category = "BR Widget|UI", meta = (WorldContext = "WorldContextObject"))
	static void ShowJoinMenu(const UObject* WorldContextObject);

	// 로비 메뉴 표시
	UFUNCTION(BlueprintCallable, Category = "BR Widget|UI", meta = (WorldContext = "WorldContextObject"))
	static void ShowLobbyMenu(const UObject* WorldContextObject);

	// 현재 메뉴 숨기기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|UI", meta = (WorldContext = "WorldContextObject"))
	static void HideCurrentMenu(const UObject* WorldContextObject);

	// VerticalBox 또는 ScrollBox에 자식 위젯 추가 (범용 함수)
	// VerticalBox와 ScrollBox 중 하나만 제공하면 됩니다 (둘 다 제공되면 VerticalBox 우선)
	UFUNCTION(BlueprintCallable, Category = "BR Widget|UI", meta = (WorldContext = "WorldContextObject"))
	static bool AddChildToContainer(const UObject* WorldContextObject, UVerticalBox* VerticalBox, UScrollBox* ScrollBox, UWidget* Content);

	// UWidget*를 받아서 자동으로 VerticalBox 또는 ScrollBox인지 판단하여 자식 추가
	UFUNCTION(BlueprintCallable, Category = "BR Widget|UI", meta = (WorldContext = "WorldContextObject"))
	static bool AddChildToContainerAuto(const UObject* WorldContextObject, UWidget* Container, UWidget* Content);
};
