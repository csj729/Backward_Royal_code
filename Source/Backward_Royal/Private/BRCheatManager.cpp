// BRCheatManager.cpp
#include "BRCheatManager.h"
#include "BRPlayerController.h"
#include "BRGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

UBRCheatManager::UBRCheatManager()
{
}

void UBRCheatManager::CreateRoom(const FString& RoomName)
{
	UE_LOG(LogTemp, Log, TEXT("[CheatManager] CreateRoom 명령 실행: %s"), *RoomName);
	
	if (APlayerController* PC = GetPlayerController())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->CreateRoom(RoomName);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CheatManager] BRPlayerController를 찾을 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRCheatManager::FindRooms()
{
	UE_LOG(LogTemp, Log, TEXT("[CheatManager] FindRooms 명령 실행"));
	
	if (APlayerController* PC = GetPlayerController())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->FindRooms();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CheatManager] BRPlayerController를 찾을 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRCheatManager::JoinRoom(int32 SessionIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[CheatManager] JoinRoom 명령 실행: SessionIndex=%d"), SessionIndex);
	
	if (APlayerController* PC = GetPlayerController())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->JoinRoom(SessionIndex);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CheatManager] BRPlayerController를 찾을 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRCheatManager::ToggleReady()
{
	UE_LOG(LogTemp, Log, TEXT("[CheatManager] ToggleReady 명령 실행"));
	
	if (APlayerController* PC = GetPlayerController())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->ToggleReady();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CheatManager] BRPlayerController를 찾을 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRCheatManager::RandomTeams()
{
	UE_LOG(LogTemp, Log, TEXT("[CheatManager] RandomTeams 명령 실행"));
	
	if (APlayerController* PC = GetPlayerController())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->RandomTeams();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CheatManager] BRPlayerController를 찾을 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRCheatManager::ChangeTeam(int32 PlayerIndex, int32 TeamNumber)
{
	UE_LOG(LogTemp, Log, TEXT("[CheatManager] ChangeTeam 명령 실행: PlayerIndex=%d, TeamNumber=%d"), PlayerIndex, TeamNumber);
	
	if (APlayerController* PC = GetPlayerController())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->ChangeTeam(PlayerIndex, TeamNumber);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CheatManager] BRPlayerController를 찾을 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRCheatManager::StartGame()
{
	UE_LOG(LogTemp, Log, TEXT("[CheatManager] StartGame 명령 실행"));
	
	if (APlayerController* PC = GetPlayerController())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->StartGame();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CheatManager] BRPlayerController를 찾을 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRCheatManager::ShowRoomInfo()
{
	UE_LOG(LogTemp, Log, TEXT("[CheatManager] ShowRoomInfo 명령 실행"));
	
	if (APlayerController* PC = GetPlayerController())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			BRPC->ShowRoomInfo();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CheatManager] BRPlayerController를 찾을 수 없습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRCheatManager::OpenListenServer()
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] OpenListenServer: PlayerController 없음"));
		return;
	}
	UWorld* World = PC->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[CheatManager] OpenListenServer: World 없음"));
		return;
	}
	ENetMode NetMode = World->GetNetMode();
	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheatManager] OpenListenServer: 이미 서버 모드(%s)입니다."),
			NetMode == NM_ListenServer ? TEXT("ListenServer") : TEXT("DedicatedServer"));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("이미 Listen Server 모드입니다."));
		return;
	}
	FString MapPath;
	if (AGameModeBase* GM = World->GetAuthGameMode())
	{
		if (ABRGameMode* BRGM = Cast<ABRGameMode>(GM))
		{
			MapPath = BRGM->LobbyMapPath;
		}
	}
	if (MapPath.IsEmpty())
	{
		MapPath = UGameplayStatics::GetCurrentLevelName(World, true);
		if (MapPath.IsEmpty())
		{
			MapPath = World->GetMapName();
			MapPath.RemoveFromStart(World->StreamingLevelsPrefix);
		}
	}
	if (MapPath.IsEmpty())
	{
		MapPath = TEXT("/Game/Main/Level/Main_Scene.Main_Scene");
	}
	// /Game/.../MapName 형식이어야 open 가능. GetCurrentLevelName은 짧은 이름만 줄 수 있음
	if (!MapPath.Contains(TEXT("/")))
	{
		MapPath = FString::Printf(TEXT("/Game/Main/Level/%s.%s"), *MapPath, *MapPath);
	}
	FString Cmd = FString::Printf(TEXT("open %s?listen"), *MapPath);
	UE_LOG(LogTemp, Warning, TEXT("[CheatManager] OpenListenServer: %s"), *Cmd);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan,
			TEXT("Listen Server로 재시작합니다. 맵 로드 후 '방 만들기'를 진행하세요."));
	}
	PC->ConsoleCommand(Cmd, /*bExecInEditor=*/false);
}

