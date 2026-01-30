// BR_SelectTeamWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BRUserInfo.h"
#include "BR_SelectTeamWidget.generated.h"

class UTextBlock;
class ABRGameState;

/**
 * 로비 SelectTeam 파츠 위젯 (WBP_SelectTeam용)
 * 팀 하나당 2슬롯(1Player=SlotIndex0, 2Player=SlotIndex1) 표시.
 * TeamIndex 0~3 = 팀1~4. OnPlayerListChanged 시 UpdateSlotDisplay 호출.
 */
UCLASS()
class BACKWARD_ROYAL_API UBR_SelectTeamWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 팀 인덱스 (0=팀1, 1=팀2, 2=팀3, 3=팀4). 블루프린트에서 설정 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Lobby")
	int32 TeamIndex = 0;

	/** 1Player 이름 표시 (BindWidgetOptional) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerNameSlot0;

	/** 2Player 이름 표시 (BindWidgetOptional) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerNameSlot1;

	/** GameState GetLobbyTeamSlotInfo(TeamIndex, 0/1)로 두 슬롯 텍스트 갱신. OnPlayerListChanged 시 호출 */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void UpdateSlotDisplay();

protected:
	virtual void NativeConstruct() override;
};
