// BRGameInstance.cpp
#include "BRGameInstance.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "BRGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

UBRGameInstance::UBRGameInstance()
{
}

void UBRGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] BRGameInstance 초기화 완료 - 콘솔 명령어 사용 가능"));
}

void UBRGameInstance::CreateRoom(const FString& RoomName)
{
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] CreateRoom 명령 실행: %s"), *RoomName);
	
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("[GameInstance] World가 없습니다. 게임을 먼저 시작해주세요."));
		return;
	}

	// PlayerController를 통한 방법
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
		{
			UE_LOG(LogTemp, Log, TEXT("[GameInstance] PlayerController를 통해 방 생성 요청"));
			BRPC->CreateRoom(RoomName);
			return;
		}
	}

	// GameMode를 통한 직접 접근 방법 (게임이 시작되지 않았을 때)
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
		{
			UE_LOG(LogTemp, Log, TEXT("[GameInstance] GameSession을 통해 직접 방 생성 요청"));
			GameSession->CreateRoomSession(RoomName);
			return;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("[GameInstance] 방 생성을 위한 필요한 객체를 찾을 수 없습니다."));
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 게임을 시작한 후 다시 시도해주세요."));
}

void UBRGameInstance::FindRooms()
{
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] FindRooms 명령"));
	
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
			{
				BRPC->FindRooms();
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 시작되지 않았을 수 있습니다."));
		}
	}
}

void UBRGameInstance::ToggleReady()
{
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] ToggleReady 명령"));
	
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
			{
				BRPC->ToggleReady();
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 시작되지 않았을 수 있습니다."));
		}
	}
}

void UBRGameInstance::RandomTeams()
{
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] RandomTeams 명령"));
	
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
			{
				BRPC->RandomTeams();
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 시작되지 않았을 수 있습니다."));
		}
	}
}

void UBRGameInstance::ChangeTeam(int32 PlayerIndex, int32 TeamNumber)
{
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] ChangeTeam 명령: PlayerIndex=%d, TeamNumber=%d"), PlayerIndex, TeamNumber);
	
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
			{
				BRPC->ChangeTeam(PlayerIndex, TeamNumber);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 시작되지 않았을 수 있습니다."));
		}
	}
}

void UBRGameInstance::StartGame()
{
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] StartGame 명령"));
	
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
			{
				BRPC->StartGame();
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 시작되지 않았을 수 있습니다."));
		}
	}
}

void UBRGameInstance::ShowRoomInfo()
{
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] ShowRoomInfo 명령"));
	
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC))
			{
				BRPC->ShowRoomInfo();
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 시작되지 않았을 수 있습니다."));
		}
	}
}

bool UBRGameInstance::GetWeaponData(FName RowName, FWeaponData& OutData)
{
	if (!WeaponDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[GameInstance] WeaponDataTable is NULL!"));
		return false;
	}

	static const FString ContextString(TEXT("Weapon Data Lookup"));
	FWeaponData* FoundRow = WeaponDataTable->FindRow<FWeaponData>(RowName, ContextString);

	if (FoundRow)
	{
		OutData = *FoundRow;
		return true;
	}

	return false;
}