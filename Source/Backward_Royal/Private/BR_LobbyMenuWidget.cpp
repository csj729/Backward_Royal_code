// BR_LobbyMenuWidget.cpp
#include "BR_LobbyMenuWidget.h"
#include "BR_LobbyTeamSlotDisplayInterface.h"
#include "BRPlayerController.h"
#include "BRGameState.h"
#include "BRPlayerState.h"
#include "BRWidgetFunctionLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Widget.h"

UBR_LobbyMenuWidget::UBR_LobbyMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CachedPlayerController(nullptr)
	, CachedGameState(nullptr)
{
}

void UBR_LobbyMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// PlayerController 캐시
	CachedPlayerController = GetBRPlayerController();

	// GameState 캐시 및 이벤트 바인딩
	CachedGameState = GetBRGameState();
	if (CachedGameState)
	{
		CachedGameState->OnPlayerListChanged.AddDynamic(this, &UBR_LobbyMenuWidget::HandlePlayerListChanged);
		CachedGameState->OnTeamChanged.AddDynamic(this, &UBR_LobbyMenuWidget::HandleTeamChanged);
		
		// 초기 게임 시작 가능 여부 확인
		HandleCanStartGameChanged();
	}

	// 초기 방 제목 표시
	FString RoomTitle = UBRWidgetFunctionLibrary::GetRoomTitleForDisplay(this);
	OnRoomTitleRefreshed(RoomTitle);
}

void UBR_LobbyMenuWidget::NativeDestruct()
{
	// 이벤트 바인딩 해제
	if (CachedGameState)
	{
		CachedGameState->OnPlayerListChanged.RemoveDynamic(this, &UBR_LobbyMenuWidget::HandlePlayerListChanged);
		CachedGameState->OnTeamChanged.RemoveDynamic(this, &UBR_LobbyMenuWidget::HandleTeamChanged);
	}

	Super::NativeDestruct();
}

void UBR_LobbyMenuWidget::ToggleReady()
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyMenu] 준비 상태 토글 요청"));
		BRPC->ToggleReady();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyMenu] PlayerController를 찾을 수 없습니다."));
	}
}

void UBR_LobbyMenuWidget::RequestRandomTeams()
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyMenu] 랜덤 팀 배정 요청"));
		BRPC->RandomTeams();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyMenu] PlayerController를 찾을 수 없습니다."));
	}
}

void UBR_LobbyMenuWidget::ChangePlayerTeam(int32 PlayerIndex, int32 TeamNumber)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyMenu] 팀 변경 요청: PlayerIndex=%d, TeamNumber=%d"), PlayerIndex, TeamNumber);
		BRPC->ChangeTeam(PlayerIndex, TeamNumber);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyMenu] PlayerController를 찾을 수 없습니다."));
	}
}

void UBR_LobbyMenuWidget::RequestStartGame()
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		UE_LOG(LogTemp, Log, TEXT("[LobbyMenu] 게임 시작 요청"));
		BRPC->StartGame();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyMenu] PlayerController를 찾을 수 없습니다."));
	}
}

ABRPlayerController* UBR_LobbyMenuWidget::GetBRPlayerController() const
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

ABRGameState* UBR_LobbyMenuWidget::GetBRGameState() const
{
	if (CachedGameState && IsValid(CachedGameState))
	{
		return CachedGameState;
	}

	// 캐시가 없거나 유효하지 않으면 새로 가져오기
	if (UWorld* World = GetWorld())
	{
		CachedGameState = World->GetGameState<ABRGameState>();
		return CachedGameState;
	}

	return nullptr;
}

ABRPlayerState* UBR_LobbyMenuWidget::GetBRPlayerState() const
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		return BRPC->GetPlayerState<ABRPlayerState>();
	}

	return nullptr;
}

bool UBR_LobbyMenuWidget::IsHost() const
{
	if (ABRPlayerState* BRPS = GetBRPlayerState())
	{
		return BRPS->bIsHost;
	}

	return false;
}

bool UBR_LobbyMenuWidget::IsReady() const
{
	if (ABRPlayerState* BRPS = GetBRPlayerState())
	{
		return BRPS->bIsReady;
	}

	return false;
}

void UBR_LobbyMenuWidget::SetMyTeamNumber(int32 TeamNumber)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		// 팀 번호 유효성 검사 (0~4)
		if (TeamNumber >= 0 && TeamNumber <= 4)
		{
			UE_LOG(LogTemp, Log, TEXT("[LobbyMenu] 팀 번호 설정: %d"), TeamNumber);
			BRPC->SetMyTeamNumber(TeamNumber);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LobbyMenu] 잘못된 팀 번호: %d (0~4 사이의 값이어야 합니다)"), TeamNumber);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyMenu] PlayerController를 찾을 수 없습니다."));
	}
}

void UBR_LobbyMenuWidget::SetMyPlayerRole(int32 PlayerIndex)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		// PlayerIndex: 0=하체, 1=상체
		if (PlayerIndex == 0 || PlayerIndex == 1)
		{
			bool bLowerBody = (PlayerIndex == 0);
			FString RoleName = bLowerBody ? TEXT("하체") : TEXT("상체");
			UE_LOG(LogTemp, Log, TEXT("[LobbyMenu] 플레이어 역할 설정: %s (PlayerIndex=%d)"), *RoleName, PlayerIndex);
			BRPC->SetMyPlayerRole(bLowerBody);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LobbyMenu] 잘못된 PlayerIndex: %d (0=하체, 1=상체)"), PlayerIndex);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyMenu] PlayerController를 찾을 수 없습니다."));
	}
}

void UBR_LobbyMenuWidget::AssignMyPlayerToTeamSlot(int32 TeamID, int32 PlayerIndex)
{
	if (ABRPlayerController* BRPC = GetBRPlayerController())
	{
		// UserInfo 기준: TeamID 1~4, PlayerIndex 0=1P, 1=2P → 내부 TeamIndex = TeamID - 1
		if (TeamID >= 1 && TeamID <= 4 && (PlayerIndex == 0 || PlayerIndex == 1))
		{
			const int32 TeamIndex = TeamID - 1;
			UE_LOG(LogTemp, Log, TEXT("[LobbyMenu] 팀 슬롯 선택: TeamID=%d, PlayerIndex=%d (%s)"), TeamID, PlayerIndex, PlayerIndex == 0 ? TEXT("1P") : TEXT("2P"));
			BRPC->RequestAssignToLobbyTeam(TeamIndex, PlayerIndex);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LobbyMenu] 잘못된 값: TeamID=%d (1~4), PlayerIndex=%d (0=1P, 1=2P)"), TeamID, PlayerIndex);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[LobbyMenu] PlayerController를 찾을 수 없습니다."));
	}
}

void UBR_LobbyMenuWidget::HandlePlayerListChanged()
{
	// 팀 슬롯(1P/2P) 자동 갱신: 인터페이스 구현체 찾아서 UpdateSlotDisplay 호출
	auto UpdateTeamSlotsInTree = [this](UWidgetTree* Tree)
	{
		if (!Tree) return;
		TArray<UWidget*> AllWidgets;
		Tree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
			{
				if (UserWidget != this && UserWidget->GetClass()->ImplementsInterface(UBR_LobbyTeamSlotDisplayInterface::StaticClass()))
				{
					IBR_LobbyTeamSlotDisplayInterface::Execute_UpdateSlotDisplay(UserWidget);
				}
			}
		}
	};

	// 1) 자신의 위젯 트리 검색
	UpdateTeamSlotsInTree(WidgetTree);

	// 2) 부모 위젯 트리도 검색 (WBP_SelectTeam이 형제 위젯일 수 있음)
	for (UWidget* Ancestor = GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
	{
		UUserWidget* ParentUserWidget = Cast<UUserWidget>(Ancestor);
		if (ParentUserWidget && ParentUserWidget->WidgetTree)
		{
			UpdateTeamSlotsInTree(ParentUserWidget->WidgetTree);
			break;
		}
	}

	// 방 제목 갱신 (C++에서 직접 호출 → 블루프린트 OnRoomTitleRefreshed 구현 필요)
	FString RoomTitle = UBRWidgetFunctionLibrary::GetRoomTitleForDisplay(this);
	OnRoomTitleRefreshed(RoomTitle);

	// 블루프린트 이벤트 호출
	OnPlayerListChanged();
}

void UBR_LobbyMenuWidget::HandleTeamChanged()
{
	// 블루프린트 이벤트 호출
	OnTeamChanged();
}

void UBR_LobbyMenuWidget::HandleCanStartGameChanged()
{
	// GameState에서 게임 시작 가능 여부 가져오기
	if (CachedGameState)
	{
		OnCanStartGameChanged(CachedGameState->bCanStartGame);
	}
}

// NativeTick를 사용하여 주기적으로 게임 시작 가능 여부 확인
void UBR_LobbyMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// GameState의 bCanStartGame이 변경되었는지 확인
	if (CachedGameState)
	{
		static bool bLastCanStartGame = false;
		if (bLastCanStartGame != CachedGameState->bCanStartGame)
		{
			bLastCanStartGame = CachedGameState->bCanStartGame;
			HandleCanStartGameChanged();
		}
	}
}
