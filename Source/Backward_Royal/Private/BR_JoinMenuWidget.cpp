// BR_JoinMenuWidget.cpp
#include "BR_JoinMenuWidget.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
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
	// Standalone 모드에서도 확인 가능하도록 화면에 큰 메시지 표시
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("[버튼 클릭 확인!] JoinRoom 함수 호출됨"));
	UE_LOG(LogTemp, Warning, TEXT("세션 인덱스: %d"), SessionIndex);
	UE_LOG(LogTemp, Warning, TEXT("========================================"));
	
	// 화면에 디버그 메시지 표시 (Standalone 모드에서도 보이도록 큰 글씨, 긴 시간)
	if (GEngine)
	{
		FString Message = FString::Printf(TEXT(">>> 버튼 클릭 확인! 방 참가 요청: 세션 인덱스 %d <<<"), SessionIndex);
		// 큰 키로 표시 (키 -1은 항상 표시, 10초 동안 표시, 밝은 녹색)
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, Message);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
	}
	
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		UE_LOG(LogTemp, Log, TEXT("[JoinMenu] PlayerController 찾음, JoinRoom 호출 중..."));
		BRPC->JoinRoom(SessionIndex);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[JoinMenu] PlayerController를 찾을 수 없습니다."));
		
		// 화면에 디버그 메시지 표시
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[JoinMenu] 실패: PlayerController를 찾을 수 없습니다!"));
		}
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
