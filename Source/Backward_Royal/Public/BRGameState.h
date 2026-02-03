// BRGameState.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BRUserInfo.h"
#include "BRGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerListChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamChanged);

UCLASS()
class BACKWARD_ROYAL_API ABRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABRGameState();

	// 최소 플레이어 수
	UPROPERTY(BlueprintReadOnly, Category = "Room Settings")
	int32 MinPlayers = 4;

	// 최대 플레이어 수
	UPROPERTY(BlueprintReadOnly, Category = "Room Settings")
	int32 MaxPlayers = 8;

	// 현재 플레이어 수
	UPROPERTY(ReplicatedUsing = OnRep_PlayerCount, BlueprintReadOnly, Category = "Room")
	int32 PlayerCount;

	/** 서버가 채운 플레이어 목록(이름 등). 복제되어 클라이언트도 동일 목록으로 UI 표시 */
	UPROPERTY(ReplicatedUsing = OnRep_PlayerListForDisplay, BlueprintReadOnly, Category = "Room")
	TArray<FBRUserInfo> PlayerListForDisplay;

	/** 로비 Entry 슬롯(0~7). 각 요소 = PlayerArray 인덱스 또는 -1(빈 슬롯). 서버에서만 수정, 복제됨 */
	UPROPERTY(ReplicatedUsing = OnRep_LobbySlots, BlueprintReadOnly, Category = "Lobby")
	TArray<int32> LobbyEntrySlots;

	/** 로비 SelectTeam 슬롯 [팀1~4][1Player/2Player]. 인덱스 = TeamIndex*2 + SlotIndex(0=1Player,1=2Player). 값 = PlayerArray 인덱스 또는 -1 */
	UPROPERTY(ReplicatedUsing = OnRep_LobbySlots, BlueprintReadOnly, Category = "Lobby")
	TArray<int32> LobbyTeamSlots;

	// 게임 시작 가능 여부
	UPROPERTY(ReplicatedUsing = OnRep_CanStartGame, BlueprintReadOnly, Category = "Room")
	bool bCanStartGame;

	// 플레이어 목록 변경 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerListChanged OnPlayerListChanged;

	// 팀 변경 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTeamChanged OnTeamChanged;

	// 플레이어 목록 업데이트
	UFUNCTION(BlueprintCallable, Category = "Room")
	void UpdatePlayerList();

	// 모든 플레이어의 UserInfo 배열 가져오기 (UI에서 사용)
	UFUNCTION(BlueprintCallable, Category = "Room")
	TArray<FBRUserInfo> GetAllPlayerUserInfo() const;

	// 특정 플레이어의 UserInfo 가져오기
	UFUNCTION(BlueprintCallable, Category = "Room")
	FBRUserInfo GetPlayerUserInfo(int32 PlayerIndex) const;

	/** 로비 Entry 표시용: 8슬롯. 빈 슬롯은 PlayerName 빈 FBRUserInfo. GetAllPlayerUserInfo 대신 로비 UI에서는 이걸 사용 권장 */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	TArray<FBRUserInfo> GetLobbyEntryDisplayList() const;

	/** 로비 SelectTeam 팀별 슬롯 표시용. TeamIndex 0~3 = 팀1~4, SlotIndex 0=1Player 1=2Player. 빈 슬롯은 PlayerName 빈 FBRUserInfo */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	FBRUserInfo GetLobbyTeamSlotInfo(int32 TeamIndex, int32 SlotIndex) const;

	/** 로비 SelectTeam 표시용. 각 플레이어의 TeamID·PlayerIndex(1P=0, 2P=1)로 찾아서 해당 슬롯의 UserInfo 반환. 복제된 PlayerState 기준이라 UI 갱신에 유리 */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	FBRUserInfo GetLobbyTeamSlotInfoByTeamIDAndPlayerIndex(int32 TeamID, int32 PlayerIndex) const;

	/** [서버 전용] 해당 플레이어를 Entry에서 제거하고 SelectTeam 슬롯에 배치. 성공 시 true */
	bool AssignPlayerToLobbyTeam(int32 PlayerIndex, int32 TeamIndex, int32 SlotIndex);
	/** [서버 전용] SelectTeam 슬롯의 플레이어를 Entry 첫 빈 자리로 이동. 성공 시 true */
	bool MovePlayerToLobbyEntry(int32 TeamIndex, int32 SlotIndex);

	/** 방장(호스트) 플레이어 이름 가져오기. "○○'s Game" 표시용 */
	UFUNCTION(BlueprintCallable, Category = "Room", meta = (DisplayName = "Get Host Player Name"))
	FString GetHostPlayerName() const;

	/** 서버에서 설정·복제되는 방 제목 (예: "○○'s Game"). 입장한 클라이언트도 동일하게 표시됨 */
	UPROPERTY(ReplicatedUsing = OnRep_RoomTitle, BlueprintReadOnly, Category = "Room")
	FString RoomTitle;

	/** 방 제목 표시용. RoomTitle이 있으면 그대로 반환, 없으면 GetHostPlayerName() + "'s Game" (블루프린트/UI에서 사용) */
	UFUNCTION(BlueprintCallable, Category = "Room", meta = (DisplayName = "Get Room Title Display"))
	FString GetRoomTitleDisplay() const;

	/** 서버 전용: 방 제목 설정 (방장 입장 시 호출) */
	void SetRoomTitle(const FString& InRoomTitle);

	// 게임 시작 가능 여부 확인
	UFUNCTION(BlueprintCallable, Category = "Room")
	void CheckCanStartGame();

	// 랜덤 팀 배정
	UFUNCTION(BlueprintCallable, Category = "Team")
	void AssignRandomTeams();

	// 모든 플레이어가 준비되었는지 확인
	UFUNCTION(BlueprintCallable, Category = "Room")
	bool AreAllPlayersReady() const;
	
	// 호스트를 제외한 모든 플레이어가 준비되었는지 확인
	UFUNCTION(BlueprintCallable, Category = "Room")
	bool AreAllNonHostPlayersReady() const;

	// 플레이어 수 변경 시 호출
	UFUNCTION()
	void OnRep_PlayerCount();

	// 복제된 플레이어 목록 수신 시 호출 (클라이언트 UI 갱신용)
	UFUNCTION()
	void OnRep_PlayerListForDisplay();

	// 로비 Entry/SelectTeam 슬롯 복제 수신 시 호출
	UFUNCTION()
	void OnRep_LobbySlots();

	// 게임 시작 가능 여부 변경 시 호출
	UFUNCTION()
	void OnRep_CanStartGame();

	// 방 제목 복제 수신 시 호출
	UFUNCTION()
	void OnRep_RoomTitle();

	/** 대기열 슬롯 압축: 빈 칸(-1) 제거 후 뒤 플레이어를 앞으로 당김. AssignPlayerToLobbyTeam / MovePlayerToLobbyEntry 후 호출 */
	void CompactLobbyEntrySlots();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
};

