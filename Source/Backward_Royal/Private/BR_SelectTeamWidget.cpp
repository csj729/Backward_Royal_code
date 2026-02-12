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

/** TeamIndex 1~4 → 0~3, 0~3은 그대로. 내부 API는 0~3만 사용 */
static int32 GetTeamIndex0Based(int32 TeamIndex)
{
	if (TeamIndex >= 1 && TeamIndex <= 4) return TeamIndex - 1;
	return FMath::Clamp(TeamIndex, 0, 3);
}

/** TeamIndex 1~4 → TeamID 1~4, 0~3 → 1~4 */
static int32 GetTeamID(int32 TeamIndex)
{
	if (TeamIndex >= 1 && TeamIndex <= 4) return TeamIndex;
	return FMath::Clamp(TeamIndex, 0, 3) + 1;
}

void UBR_SelectTeamWidget::RequestAssignToTeamSlotSpectator()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->RequestAssignToLobbyTeam(GetTeamIndex0Based(TeamIndex), 0);  // SlotIndex 0 = 관전
			ScheduleSlotDisplayRefresh();
		}
	}
}

void UBR_SelectTeamWidget::RequestAssignToTeamSlot1P()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->RequestAssignToLobbyTeam(GetTeamIndex0Based(TeamIndex), 1);  // SlotIndex 1 = 하체(1P)
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
			BRPC->RequestAssignToLobbyTeam(GetTeamIndex0Based(TeamIndex), 2);  // SlotIndex 2 = 상체(2P)
			ScheduleSlotDisplayRefresh();
		}
	}
}

void UBR_SelectTeamWidget::ScheduleSlotDisplayRefresh()
{
	// 즉시 한 번 갱신 (서버/호스트는 이미 반영된 상태에서 UI 갱신, 두 번째부터 이름 안 나오는 현상 완화)
	UpdateSlotDisplay();

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

	const int32 TeamIndex0 = GetTeamIndex0Based(TeamIndex);
	const int32 TeamID = GetTeamID(TeamIndex);
	// SlotIndex 0=관전, 1=하체, 2=상체
	FBRUserInfo Info0 = GS->GetLobbyTeamSlotInfo(TeamIndex0, 0);
	FBRUserInfo Info1 = GS->GetLobbyTeamSlotInfo(TeamIndex0, 1);
	FBRUserInfo Info2 = GS->GetLobbyTeamSlotInfo(TeamIndex0, 2);
	if (UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info0).IsEmpty())
		Info0 = GS->GetLobbyTeamSlotInfoByTeamIDAndPlayerIndex(TeamID, 0);
	if (UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info1).IsEmpty())
		Info1 = GS->GetLobbyTeamSlotInfoByTeamIDAndPlayerIndex(TeamID, 1);
	if (UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info2).IsEmpty())
		Info2 = GS->GetLobbyTeamSlotInfoByTeamIDAndPlayerIndex(TeamID, 2);
	FString Display0 = UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info0);
	FString Display1 = UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info1);
	FString Display2 = UBRWidgetFunctionLibrary::GetDisplayNameForLobby(Info2);

	if (PlayerNameSlot0)
		PlayerNameSlot0->SetText(Display0.IsEmpty() ? FText::FromString(TEXT("관전")) : FText::FromString(Display0));
	if (PlayerNameSlot1)
		PlayerNameSlot1->SetText(Display1.IsEmpty() ? FText::FromString(TEXT("1Player")) : FText::FromString(Display1));
	if (PlayerNameSlot2)
		PlayerNameSlot2->SetText(Display2.IsEmpty() ? FText::FromString(TEXT("2Player")) : FText::FromString(Display2));
}
