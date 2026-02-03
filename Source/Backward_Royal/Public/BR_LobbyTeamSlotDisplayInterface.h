// BR_LobbyTeamSlotDisplayInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BR_LobbyTeamSlotDisplayInterface.generated.h"

/**
 * 로비 팀 슬롯(1P/2P) 표시를 갱신하는 위젯이 구현하는 인터페이스.
 * WBP_SelectTeam이 BR_SelectTeamWidget을 부모로 두지 않아도, 이 인터페이스만 구현하면 됨.
 */
UINTERFACE(Blueprintable, MinimalAPI)
class UBR_LobbyTeamSlotDisplayInterface : public UInterface
{
	GENERATED_BODY()
};

class BACKWARD_ROYAL_API IBR_LobbyTeamSlotDisplayInterface
{
	GENERATED_BODY()

public:
	/** 팀 슬롯(1P/2P) 이름 표시 갱신. 블루프린트에서 GameState GetLobbyTeamSlotInfo(TeamIndex, 0/1) → 텍스트 설정 등 구현 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lobby")
	void UpdateSlotDisplay();
	virtual void UpdateSlotDisplay_Implementation() {}
};
