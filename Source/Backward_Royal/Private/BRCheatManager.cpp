// BRCheatManager.cpp
#include "BRCheatManager.h"
#include "BRPlayerController.h"

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

