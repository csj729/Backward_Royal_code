// BR_EntranceMenuWidget.cpp
#include "BR_EntranceMenuWidget.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"

UBR_EntranceMenuWidget::UBR_EntranceMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CachedPlayerController(nullptr)
{
}

void UBR_EntranceMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// PlayerController 캐시
	CachedPlayerController = GetBRPlayerController();

	// GameSession 이벤트 바인딩 (방 생성 완료)
	if (UWorld* World = GetWorld())
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
			{
				GameSession->OnCreateSessionComplete.AddDynamic(this, &UBR_EntranceMenuWidget::HandleCreateRoomComplete);
			}
		}
	}
}

void UBR_EntranceMenuWidget::CreateRoom(const FString& RoomName)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		UE_LOG(LogTemp, Log, TEXT("[EntranceMenu] 방 생성 요청: %s"), *RoomName);
		BRPC->CreateRoom(RoomName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[EntranceMenu] PlayerController를 찾을 수 없습니다."));
	}
}

void UBR_EntranceMenuWidget::FindRooms()
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		UE_LOG(LogTemp, Log, TEXT("[EntranceMenu] 방 찾기 요청"));
		BRPC->FindRooms();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[EntranceMenu] PlayerController를 찾을 수 없습니다."));
	}
}

ABRPlayerController* UBR_EntranceMenuWidget::GetBRPlayerController() const
{
	if (CachedPlayerController && IsValid(CachedPlayerController))
	{
		return CachedPlayerController;
	}

	// 캐시가 없거나 유효하지 않으면 새로 가져오기
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			CachedPlayerController = Cast<ABRPlayerController>(PC);
			return CachedPlayerController;
		}
	}

	return nullptr;
}

void UBR_EntranceMenuWidget::HandleCreateRoomComplete(bool bWasSuccessful)
{
	// 블루프린트 이벤트 호출
	OnCreateRoomComplete(bWasSuccessful);
}
