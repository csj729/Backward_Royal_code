// BR_LobbyEntryWidget.cpp
#include "BR_LobbyEntryWidget.h"
#include "Components/TextBlock.h"

void UBR_LobbyEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBR_LobbyEntryWidget::UpdatePlayerNames(const TArray<FBRUserInfo>& PlayerInfoList)
{
	// 1. 모든 슬롯 초기화 (빈 텍스트 = 공란)
	for (UTextBlock* TextSlot : UserNameSlot)
	{
		if (TextSlot)
		{
			TextSlot->SetText(FText::GetEmpty());
		}
	}

	// 2. 들어온 순서(PlayerIndex 0, 1, 2, …)대로 슬롯에 이름 표시. 해당 인덱스에 플레이어 없으면 공란 유지
	for (int32 SlotIndex = 0; SlotIndex < UserNameSlot.Num(); ++SlotIndex)
	{
		if (!UserNameSlot[SlotIndex])
		{
			continue;
		}
		if (SlotIndex < PlayerInfoList.Num())
		{
			const FBRUserInfo& Info = PlayerInfoList[SlotIndex];
			// 빈 슬롯(PlayerIndex < 0) 또는 이름 미설정 → 공란. 플레이어가 있으면 PlayerName만 표시
			FString DisplayName;
			if (Info.PlayerIndex < 0)
			{
				DisplayName = FString();
			}
			else if (!Info.PlayerName.IsEmpty() && Info.PlayerName != Info.UserUID)
			{
				DisplayName = Info.PlayerName;
			}
			// else: 이름 없음 → 공란 유지
			UserNameSlot[SlotIndex]->SetText(FText::FromString(DisplayName));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LobbyEntry] 입장 순서 기준 %d슬롯 표시 (플레이어 %d명)"), UserNameSlot.Num(), PlayerInfoList.Num());
}

void UBR_LobbyEntryWidget::SetEntryInfo(const FBRUserInfo& Info)
{
	if (!NameText)
	{
		return;
	}

	// 빈 슬롯 또는 이름 미설정 → 공란. 플레이어가 있으면 PlayerName만 표시
	FString DisplayName;
	if (Info.PlayerIndex >= 0 && !Info.PlayerName.IsEmpty() && Info.PlayerName != Info.UserUID)
	{
		DisplayName = Info.PlayerName;
	}
	NameText->SetText(FText::FromString(DisplayName));
}
