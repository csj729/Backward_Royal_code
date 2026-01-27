// BR_EntranceMenuWidget.cpp
#include "BR_EntranceMenuWidget.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
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
	UE_LOG(LogTemp, Log, TEXT("[EntranceMenu] 방 생성 완료: %s"), bWasSuccessful ? TEXT("성공") : TEXT("실패"));
	
	// 화면에 디버그 메시지 표시 (Standalone 모드에서도 확인 가능)
	if (GEngine)
	{
		FString Message = bWasSuccessful ? 
			TEXT("[EntranceMenu] 방 생성 성공! 위젯 제거 중...") : 
			TEXT("[EntranceMenu] 방 생성 실패!");
		FColor MessageColor = bWasSuccessful ? FColor::Green : FColor::Red;
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, MessageColor, Message);
	}
	
	if (bWasSuccessful)
	{
		// 방 생성 성공 시 EntranceMenu 완전히 제거
		// (맵이 다시 로드되면 BeginPlay에서 ListenServer 모드로 감지하여 LobbyMenu가 표시됨)
		RemoveFromParent();
		UE_LOG(LogTemp, Log, TEXT("[EntranceMenu] 방 생성 성공 - EntranceMenu 제거"));
		
		// PlayerController의 CurrentMenuWidget도 정리
		if (ABRPlayerController* BRPC = GetBRPlayerController())
		{
			BRPC->HideCurrentMenu();
		}
	}
	
	// 블루프린트 이벤트 호출 (블루프린트에서 추가 처리 가능)
	// 단, 방 생성 성공 시에는 블루프린트 이벤트를 호출하지 않음 (위젯이 제거되므로)
	if (!bWasSuccessful)
	{
		OnCreateRoomComplete(bWasSuccessful);
	}
}
