// BRWidgetFunctionLibrary.cpp
#include "BRWidgetFunctionLibrary.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"

ABRPlayerController* UBRWidgetFunctionLibrary::GetBRPlayerController(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		return Cast<ABRPlayerController>(PC);
	}

	return nullptr;
}

void UBRWidgetFunctionLibrary::CreateRoom(const UObject* WorldContextObject, const FString& RoomName)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 방 생성 요청: %s"), *RoomName);
		BRPC->CreateRoom(RoomName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::FindRooms(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 방 찾기 요청"));
		BRPC->FindRooms();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::JoinRoom(const UObject* WorldContextObject, int32 SessionIndex)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 방 참가 요청: 세션 인덱스=%d"), SessionIndex);
		BRPC->JoinRoom(SessionIndex);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::ToggleReady(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 준비 상태 토글 요청"));
		BRPC->ToggleReady();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::RandomTeams(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 랜덤 팀 배정 요청"));
		BRPC->RandomTeams();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::ChangeTeam(const UObject* WorldContextObject, int32 PlayerIndex, int32 TeamNumber)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 팀 변경 요청: PlayerIndex=%d, TeamNumber=%d"), PlayerIndex, TeamNumber);
		BRPC->ChangeTeam(PlayerIndex, TeamNumber);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::StartGame(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 게임 시작 요청"));
		BRPC->StartGame();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

ABRGameSession* UBRWidgetFunctionLibrary::GetBRGameSession(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	if (AGameModeBase* GameMode = World->GetAuthGameMode())
	{
		return Cast<ABRGameSession>(GameMode->GameSession);
	}

	return nullptr;
}

ABRGameState* UBRWidgetFunctionLibrary::GetBRGameState(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	return World->GetGameState<ABRGameState>();
}

ABRPlayerState* UBRWidgetFunctionLibrary::GetBRPlayerState(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		return BRPC->GetPlayerState<ABRPlayerState>();
	}

	return nullptr;
}

bool UBRWidgetFunctionLibrary::IsHost(const UObject* WorldContextObject)
{
	if (ABRPlayerState* BRPS = GetBRPlayerState(WorldContextObject))
	{
		return BRPS->bIsHost;
	}

	return false;
}

bool UBRWidgetFunctionLibrary::IsReady(const UObject* WorldContextObject)
{
	if (ABRPlayerState* BRPS = GetBRPlayerState(WorldContextObject))
	{
		return BRPS->bIsReady;
	}

	return false;
}
