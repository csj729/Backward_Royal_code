// BRWidgetFunctionLibrary.cpp
#include "BRWidgetFunctionLibrary.h"
#include "BRPlayerController.h"
#include "BRGameInstance.h"
#include "BRGameSession.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Widget.h"

ABRPlayerController* UBRWidgetFunctionLibrary::GetBRPlayerController(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] GetBRPlayerController: WorldContextObject가 nullptr입니다."));
		return nullptr;
	}

	UWorld* World = nullptr;
	
	// 여러 방법으로 World 가져오기 시도
	if (GEngine)
	{
		World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	}
	
	// WorldContextObject가 UWorld를 직접 상속하는 경우
	if (!World && WorldContextObject->IsA<UWorld>())
	{
		World = const_cast<UWorld*>(Cast<UWorld>(WorldContextObject));
	}
	
	// WorldContextObject가 UObject이고 Outer를 통해 World를 찾는 경우
	if (!World && WorldContextObject)
	{
		World = WorldContextObject->GetWorld();
	}
	
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] GetBRPlayerController: World를 찾을 수 없습니다."));
		return nullptr;
	}

	// PlayerController 찾기 시도
	APlayerController* PC = nullptr;
	
	// 방법 1: UGameplayStatics 사용
	PC = UGameplayStatics::GetPlayerController(World, 0);
	
	// 방법 2: World에서 직접 가져오기
	if (!PC && World->GetFirstPlayerController())
	{
		PC = World->GetFirstPlayerController();
	}
	
	// 방법 3: LocalPlayer를 통해 가져오기
	if (!PC && GEngine)
	{
		if (ULocalPlayer* LocalPlayer = GEngine->GetFirstGamePlayer(World))
		{
			PC = LocalPlayer->GetPlayerController(World);
		}
	}
	
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] GetBRPlayerController: PlayerController를 찾을 수 없습니다. (World: %s)"), 
			World ? *World->GetName() : TEXT("None"));
		return nullptr;
	}

	ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC);
	if (!BRPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WidgetFunctionLibrary] GetBRPlayerController: PlayerController를 BRPlayerController로 캐스팅할 수 없습니다. (Type: %s)"), 
			*PC->GetClass()->GetName());
		return nullptr;
	}

	return BRPC;
}

bool UBRWidgetFunctionLibrary::GetBRPlayerControllerSafe(const UObject* WorldContextObject, ABRPlayerController*& OutPlayerController)
{
	OutPlayerController = GetBRPlayerController(WorldContextObject);
	return OutPlayerController != nullptr;
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

void UBRWidgetFunctionLibrary::SetUseLANOnly(const UObject* WorldContextObject, bool bLAN)
{
	if (!WorldContextObject || !GEngine) return;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return;
	UGameInstance* GI = World->GetGameInstance();
	if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(GI))
	{
		BRGI->SetUseLANOnly(bLAN);
		UE_LOG(LogTemp, Log, TEXT("[WidgetFunctionLibrary] SetUseLANOnly: %s"), bLAN ? TEXT("LAN 전용") : TEXT("인터넷 매칭"));
	}
}

bool UBRWidgetFunctionLibrary::GetUseLANOnly(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine) return true;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return true;
	UGameInstance* GI = World->GetGameInstance();
	if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(GI))
	{
		return BRGI->GetUseLANOnly();
	}
	return true;
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
// 네트워크 연결 상태 확인 함수 구현
// ============================================

bool UBRWidgetFunctionLibrary::IsConnectedToServer(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine) return false;
	
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return false;
	
	ENetMode NetMode = World->GetNetMode();
	
	// 클라이언트 모드인 경우에만 연결 확인
	if (NetMode == NM_Client)
	{
		// PlayerController를 통해 연결 확인 (더 안전한 방법)
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			// GetNetConnection()을 사용하여 연결 확인
			if (UNetConnection* Connection = PC->GetNetConnection())
			{
				// 연결이 존재하면 연결된 것으로 간주
				// State는 private이므로 연결 존재 여부만 확인
				return Connection != nullptr;
			}
		}
		
		// 대체 방법: NetDriver를 통한 확인
		if (UNetDriver* NetDriver = World->GetNetDriver())
		{
			// ServerConnection이 존재하면 연결된 것으로 간주
			// State는 private이므로 연결 존재 여부만 확인
			return NetDriver->ServerConnection != nullptr;
		}
		return false;
	}
	
	// 서버 모드(ListenServer/DedicatedServer)는 항상 "연결됨"으로 간주
	return (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer);
}

FString UBRWidgetFunctionLibrary::GetNetworkMode(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine) return TEXT("Unknown");
	
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return TEXT("Unknown");
	
	ENetMode NetMode = World->GetNetMode();
	switch (NetMode)
	{
	case NM_Standalone: return TEXT("Standalone");
	case NM_Client: return TEXT("Client");
	case NM_ListenServer: return TEXT("ListenServer");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	default: return TEXT("Unknown");
	}
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