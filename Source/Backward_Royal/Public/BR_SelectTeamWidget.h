// BR_SelectTeamWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "BRUserInfo.h"
#include "BR_LobbyTeamSlotDisplayInterface.h"
#include "BR_SelectTeamWidget.generated.h"

class UTextBlock;
class ABRGameState;

/**
 * 로비 SelectTeam 파츠 위젯 (WBP_SelectTeam용)
 * 팀 하나당 3슬롯: SlotIndex 0=관전, 1=1Player(하체), 2=2Player(상체).
 * TeamIndex: 1=1팀, 2=2팀, 3=3팀, 4=4팀 (권장). 0~3도 호환.
 * OnPlayerListChanged 시 UpdateSlotDisplay 호출.
 */
UCLASS()
class BACKWARD_ROYAL_API UBR_SelectTeamWidget : public UUserWidget, public IBR_LobbyTeamSlotDisplayInterface
{
	GENERATED_BODY()

public:
	/** 팀 번호. 1=1팀, 2=2팀, 3=3팀, 4=4팀 (권장). 0~3도 지원(0=1팀, 1=2팀, ...). 블루프린트에서 설정 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Lobby")
	int32 TeamIndex = 1;

	/** 관전 슬롯 이름 표시 (BindWidgetOptional). SlotIndex 0 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerNameSlot0;

	/** 1Player(하체) 이름 표시 (BindWidgetOptional). SlotIndex 1 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerNameSlot1;

	/** 2Player(상체) 이름 표시 (BindWidgetOptional). SlotIndex 2 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerNameSlot2;

	/** 이 팀의 관전 슬롯 클릭 시 호출. SlotIndex 0 = 관전 */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestAssignToTeamSlotSpectator();

	/** 이 팀의 1P(하체) 버튼 클릭 시 호출. SlotIndex 1 → UserInfo PlayerIndex 1 */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestAssignToTeamSlot1P();

	/** 이 팀의 2P(상체) 버튼 클릭 시 호출. SlotIndex 2 → UserInfo PlayerIndex 2 */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestAssignToTeamSlot2P();

	/** GameState GetLobbyTeamSlotInfo(TeamIndex, 0/1/2)로 세 슬롯 텍스트 갱신. OnPlayerListChanged 시 호출 */


	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lobby")
	void UpdateSlotDisplay();

	// IBR_LobbyTeamSlotDisplayInterface
	virtual void UpdateSlotDisplay_Implementation() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** OnPlayerListChanged 바인딩 해제용 */
	UFUNCTION()
	void HandlePlayerListChanged();

	/** 버튼 클릭 후 복제 도착 뒤 1P/2P 이름 갱신을 위해 짧은 지연 후 UpdateSlotDisplay 호출 */
	void ScheduleSlotDisplayRefresh();

	UPROPERTY()
	TObjectPtr<ABRGameState> CachedGameState;
};
