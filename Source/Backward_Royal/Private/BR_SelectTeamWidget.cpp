// BR_SelectTeamWidget.cpp
#include "BR_SelectTeamWidget.h"
#include "BRGameState.h"
#include "BRWidgetFunctionLibrary.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void UBR_SelectTeamWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateSlotDisplay();
}

void UBR_SelectTeamWidget::UpdateSlotDisplay()
{
	UWorld* World = GetWorld();
	if (!World) return;
	ABRGameState* GS = World->GetGameState<ABRGameState>();
	if (!GS) return;

	FBRUserInfo Info0 = GS->GetLobbyTeamSlotInfo(TeamIndex, 0);
	FBRUserInfo Info1 = GS->GetLobbyTeamSlotInfo(TeamIndex, 1);
	FString Display0 = UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info0);
	FString Display1 = UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info1);

	if (PlayerNameSlot0)
	{
		PlayerNameSlot0->SetText(Display0.IsEmpty() ? FText::GetEmpty() : FText::FromString(Display0));
	}
	if (PlayerNameSlot1)
	{
		PlayerNameSlot1->SetText(Display1.IsEmpty() ? FText::GetEmpty() : FText::FromString(Display1));
	}
}
