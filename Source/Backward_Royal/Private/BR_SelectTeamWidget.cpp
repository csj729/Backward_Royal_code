// BR_SelectTeamWidget.cpp
#include "BR_SelectTeamWidget.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "BRWidgetFunctionLibrary.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void UBR_SelectTeamWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CachedGameState = GetWorld() ? GetWorld()->GetGameState<ABRGameState>() : nullptr;
	if (CachedGameState)
	{
		CachedGameState->OnPlayerListChanged.AddDynamic(this, &UBR_SelectTeamWidget::HandlePlayerListChanged);
	}
	UpdateSlotDisplay();
}

void UBR_SelectTeamWidget::NativeDestruct()
{
	if (CachedGameState)
	{
		CachedGameState->OnPlayerListChanged.RemoveDynamic(this, &UBR_SelectTeamWidget::HandlePlayerListChanged);
		CachedGameState = nullptr;
	}
	Super::NativeDestruct();
}

void UBR_SelectTeamWidget::HandlePlayerListChanged()
{
	UpdateSlotDisplay();
}

void UBR_SelectTeamWidget::RequestAssignToTeamSlot1P()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->RequestAssignToLobbyTeam(TeamIndex, 0);  // 1P = SlotIndex 0 → UserInfo PlayerIndex 0
			ScheduleSlotDisplayRefresh();
		}
	}
}

void UBR_SelectTeamWidget::RequestAssignToTeamSlot2P()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->RequestAssignToLobbyTeam(TeamIndex, 1);  // 2P = SlotIndex 1 → UserInfo PlayerIndex 1
			ScheduleSlotDisplayRefresh();
		}
	}
}

void UBR_SelectTeamWidget::ScheduleSlotDisplayRefresh()
{
	UWorld* World = GetWorld();
	if (!World) return;
	TWeakObjectPtr<UBR_SelectTeamWidget> WeakThis(this);
	// 복제 도착 후 1P/2P 이름 갱신: 0.25초·0.6초 두 번 호출해 지연 복제에도 대응
	auto Refresh = [WeakThis]() { if (UBR_SelectTeamWidget* W = WeakThis.Get()) W->UpdateSlotDisplay(); };
	FTimerHandle H1, H2;
	World->GetTimerManager().SetTimer(H1, [WeakThis, Refresh]() { Refresh(); }, 0.25f, false);
	World->GetTimerManager().SetTimer(H2, [WeakThis, Refresh]() { Refresh(); }, 0.6f, false);
}

void UBR_SelectTeamWidget::UpdateSlotDisplay_Implementation()
{
	UWorld* World = GetWorld();
	if (!World) return;
	ABRGameState* GS = World->GetGameState<ABRGameState>();
	if (!GS) return;

	const int32 TeamID = TeamIndex + 1;
	// 1) LobbyTeamSlots + PlayerListForDisplay 우선 (복제 후 이름이 안정적으로 나옴)
	FBRUserInfo Info0 = GS->GetLobbyTeamSlotInfo(TeamIndex, 0);
	FBRUserInfo Info1 = GS->GetLobbyTeamSlotInfo(TeamIndex, 1);
	// 2) 이름이 비어 있으면 PlayerState(TeamID·PlayerIndex) 기준 폴백
	if (UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info0).IsEmpty())
	{
		Info0 = GS->GetLobbyTeamSlotInfoByTeamIDAndPlayerIndex(TeamID, 0);
	}
	if (UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info1).IsEmpty())
	{
		Info1 = GS->GetLobbyTeamSlotInfoByTeamIDAndPlayerIndex(TeamID, 1);
	}
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
