// BRWidgetFunctionLibrary.cpp
#include "BRWidgetFunctionLibrary.h"
#include "BRPlayerController.h"
#include "BRGameInstance.h"
#include "BRGameSession.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Widget.h"

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
	UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] JoinRoom 함수 호출됨: 세션 인덱스=%d"), SessionIndex);
	
	if (GEngine)
	{
		FString Message = FString::Printf(TEXT("[WidgetFunctionLibrary] 방 참가 요청: 세션 인덱스 %d"), SessionIndex);
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Message);
	}
	
	ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject);
	if (!BRPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[WidgetFunctionLibrary] 실패: PlayerController를 찾을 수 없습니다!"));
		}
		return;
	}

	// Game Instance에서 Player Name 조회 (블루프린트 Cast/Get Game Instance 불필요)
	FString PlayerName;
	if (UGameInstance* GI = BRPC->GetGameInstance())
	{
		if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(GI))
		{
			PlayerName = BRGI->GetPlayerName();
			UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] Game Instance에서 플레이어 이름 조회: %s"), *PlayerName);
		}
	}
	if (PlayerName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] Player Name 없음, 빈 문자열로 JoinRoom 호출"));
	}

	BRPC->JoinRoomWithPlayerName(SessionIndex, PlayerName);
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

// ============================================
// UI 관련 함수 구현
// ============================================

void UBRWidgetFunctionLibrary::ShowMainScreen(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] MainScreen 표시 요청"));
		BRPC->ShowMainScreen();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::ShowEntranceMenu(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] EntranceMenu 표시 요청"));
		BRPC->ShowEntranceMenu();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::ShowJoinMenu(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] JoinMenu 표시 요청"));
		BRPC->ShowJoinMenu();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::ShowLobbyMenu(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] LobbyMenu 표시 요청"));
		BRPC->ShowLobbyMenu();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

void UBRWidgetFunctionLibrary::HideCurrentMenu(const UObject* WorldContextObject)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController(WorldContextObject))
	{
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 현재 메뉴 숨기기 요청"));
		BRPC->HideCurrentMenu();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[WidgetFunctionLibrary] PlayerController를 찾을 수 없습니다."));
	}
}

bool UBRWidgetFunctionLibrary::AddChildToContainer(const UObject* WorldContextObject, UVerticalBox* VerticalBox, UScrollBox* ScrollBox, UWidget* Content)
{
	if (!Content)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] AddChildToContainer: Content가 유효하지 않습니다."));
		return false;
	}

	// Standalone 모드에서 위젯이 입력을 받을 수 있도록 OwningPlayer 설정
	if (UUserWidget* UserWidget = Cast<UUserWidget>(Content))
	{
		if (UserWidget->GetOwningPlayer() == nullptr)
		{
			// WorldContextObject에서 PlayerController 가져오기
			if (WorldContextObject)
			{
				UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
				if (World)
				{
					if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
					{
						UserWidget->SetOwningPlayer(PC);
						UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 위젯 OwningPlayer 설정 완료 (Standalone 모드 대응)"));
					}
				}
			}
		}
	}

	// VerticalBox에 추가 (우선순위)
	if (VerticalBox && IsValid(VerticalBox))
	{
		VerticalBox->AddChildToVerticalBox(Content);
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] VerticalBox에 자식 위젯 추가 완료"));
		return true;
	}

	// ScrollBox에 추가
	if (ScrollBox && IsValid(ScrollBox))
	{
		ScrollBox->AddChild(Content);
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] ScrollBox에 자식 위젯 추가 완료"));
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] AddChildToContainer: VerticalBox 또는 ScrollBox가 유효하지 않습니다."));
	return false;
}

bool UBRWidgetFunctionLibrary::AddChildToContainerAuto(const UObject* WorldContextObject, UWidget* Container, UWidget* Content)
{
	if (!Content)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] AddChildToContainerAuto: Content가 유효하지 않습니다."));
		return false;
	}

	if (!Container || !IsValid(Container))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] AddChildToContainerAuto: Container가 유효하지 않습니다."));
		return false;
	}

	// Standalone 모드에서 위젯이 입력을 받을 수 있도록 OwningPlayer 설정
	if (UUserWidget* UserWidget = Cast<UUserWidget>(Content))
	{
		if (UserWidget->GetOwningPlayer() == nullptr)
		{
			// WorldContextObject에서 PlayerController 가져오기
			if (WorldContextObject)
			{
				UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
				if (World)
				{
					if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
					{
						UserWidget->SetOwningPlayer(PC);
						UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] 위젯 OwningPlayer 설정 완료 (Standalone 모드 대응)"));
					}
				}
			}
		}
	}

	// VerticalBox인지 확인
	if (UVerticalBox* VerticalBox = Cast<UVerticalBox>(Container))
	{
		VerticalBox->AddChildToVerticalBox(Content);
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] VerticalBox에 자식 위젯 추가 완료 (자동 감지)"));
		return true;
	}

	// ScrollBox인지 확인
	if (UScrollBox* ScrollBox = Cast<UScrollBox>(Container))
	{
		ScrollBox->AddChild(Content);
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] ScrollBox에 자식 위젯 추가 완료 (자동 감지)"));
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] AddChildToContainerAuto: Container가 VerticalBox 또는 ScrollBox가 아닙니다."));
	return false;
}