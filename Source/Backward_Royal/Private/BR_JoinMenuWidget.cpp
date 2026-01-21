// BR_JoinMenuWidget.cpp
#include "BR_JoinMenuWidget.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"

UBR_JoinMenuWidget::UBR_JoinMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CachedPlayerController(nullptr)
	, CachedGameSession(nullptr)
{
}

void UBR_JoinMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// PlayerController 캐시
	CachedPlayerController = GetBRPlayerController();

	// GameSession 캐시 및 이벤트 바인딩
	CachedGameSession = GetBRGameSession();
	if (CachedGameSession)
	{
		CachedGameSession->OnJoinSessionComplete.AddDynamic(this, &UBR_JoinMenuWidget::HandleJoinSessionComplete);
	}
}

void UBR_JoinMenuWidget::NativeDestruct()
{
	// 이벤트 바인딩 해제
	if (CachedGameSession)
	{
		CachedGameSession->OnJoinSessionComplete.RemoveDynamic(this, &UBR_JoinMenuWidget::HandleJoinSessionComplete);
	}

	Super::NativeDestruct();
}

void UBR_JoinMenuWidget::JoinRoom(int32 SessionIndex)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		UE_LOG(LogTemp, Log, TEXT("[JoinMenu] 방 참가 요청: 세션 인덱스=%d"), SessionIndex);
		BRPC->JoinRoom(SessionIndex);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[JoinMenu] PlayerController를 찾을 수 없습니다."));
	}
}

void UBR_JoinMenuWidget::RefreshRoomList()
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		UE_LOG(LogTemp, Log, TEXT("[JoinMenu] 방 목록 새로고침"));
		BRPC->FindRooms();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[JoinMenu] PlayerController를 찾을 수 없습니다."));
	}
}

ABRPlayerController* UBR_JoinMenuWidget::GetBRPlayerController() const
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

ABRGameSession* UBR_JoinMenuWidget::GetBRGameSession() const
{
	if (CachedGameSession && IsValid(CachedGameSession))
	{
		return CachedGameSession;
	}

	// 캐시가 없거나 유효하지 않으면 새로 가져오기
	if (UWorld* World = GetWorld())
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			CachedGameSession = Cast<ABRGameSession>(GameMode->GameSession);
			return CachedGameSession;
		}
	}

	return nullptr;
}

void UBR_JoinMenuWidget::HandleJoinSessionComplete(bool bWasSuccessful)
{
	// 블루프린트 이벤트 호출
	OnJoinRoomComplete(bWasSuccessful);
}
