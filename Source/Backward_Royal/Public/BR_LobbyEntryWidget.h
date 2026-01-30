// BR_LobbyEntryWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BRUserInfo.h"
#include "BR_LobbyEntryWidget.generated.h"

class UTextBlock;

/**
 * 로비 플레이어 엔트리 위젯 베이스 클래스 (WBP_Entry용)
 * 들어온 순서(0번, 1번, …)대로 UserNameSlot에 플레이어 이름을 표시합니다.
 * 해당 순서에 플레이어가 없으면 빈 슬롯은 공란으로 둡니다.
 */
UCLASS()
class BACKWARD_ROYAL_API UBR_LobbyEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 들어온 순서(0번, 1번, …)대로 UserNameSlot에 플레이어 이름 표시.
	 * 해당 인덱스에 플레이어가 없으면 공란 유지.
	 * @param PlayerInfoList GameState::GetAllPlayerUserInfo() 등으로 얻은 플레이어 목록 (입장 순서)
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby Entry")
	void UpdatePlayerNames(const TArray<FBRUserInfo>& PlayerInfoList);

	/**
	 * 단일 엔트리 정보 설정 (하위 호환용).
	 * NameText에 플레이어 이름을 표시합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby Entry")
	void SetEntryInfo(const FBRUserInfo& Info);

	/** 이름 표시용 TextBlock 배열 (블루프린트에서 설정) */
	UPROPERTY(BlueprintReadWrite, Category = "Lobby Entry")
	TArray<TObjectPtr<UTextBlock>> UserNameSlot;

	/** 단일 NameText 블록 (BindWidgetOptional, 하위 호환용) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

protected:
	virtual void NativeConstruct() override;
};
