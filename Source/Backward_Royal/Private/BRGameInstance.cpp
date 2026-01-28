// BRGameInstance.cpp
#include "BRGameInstance.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "BRGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "BaseWeapon.h"
#include "GlobalBalanceData.h"
#include "UObject/Package.h"
#include "PlayerCharacter.h"
#include "StaminaComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Subsystems/WorldSubsystem.h"

#if WITH_EDITOR
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY(LogBRGameInstance);

#define GI_LOG(Verbosity, Format, ...) UE_LOG(LogBRGameInstance, Verbosity, TEXT("%s: ") Format, *FString(__FUNCTION__), ##__VA_ARGS__)

UBRGameInstance::UBRGameInstance()
{
}

void UBRGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogTemp, Log, TEXT("[GameInstance] BRGameInstance 초기화 완료 - 콘솔 명령어 사용 가능"));
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 네트워크 모드: %s"), 
		bUseLANOnly ? TEXT("LAN 전용") : TEXT("인터넷 매칭 (Steam)"));
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 모드 변경: 콘솔에서 'SetLANOnly 1' (LAN) 또는 'SetLANOnly 0' (인터넷)"));
	
	// PIE 월드 클린업이 엔진의 '월드 참조 검사'보다 먼저 일어나게 등록.
	// FDelegateHandle/Delegate 헤더 경로 이슈를 피하기 위해 해제하지 않고, 콜백에서 TWeakObjectPtr로만 판별.
	TWeakObjectPtr<UBRGameInstance> Self(this);
	FWorldDelegates::OnWorldCleanup.AddLambda([Self](UWorld* InWorld, bool bSessionEnding, bool bCleanupResources)
	{
		if (InWorld && InWorld->IsPlayInEditor() && Self.IsValid() && InWorld->GetGameInstance() == Self.Get())
		{
			Self->DoPIEExitCleanup(InWorld);
		}
	});
	
	// 패킹된 게임에서 Standalone 모드로 시작하는 것을 방지하기 위해
	// 명령줄 인자 확인 (이미 ?listen이 있으면 그대로 사용)
	FString CommandLine = FCommandLine::Get();
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 명령줄: %s"), *CommandLine);
	
	ReloadAllConfigs();
}

void UBRGameInstance::OnStart()
{
	Super::OnStart();
	
	UE_LOG(LogTemp, Error, TEXT("========================================"));
	UE_LOG(LogTemp, Error, TEXT("[GameInstance] OnStart 호출 - 첫 번째 World 생성 완료"));
	UE_LOG(LogTemp, Error, TEXT("========================================"));
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] PendingRoomName 상태 확인: %s"), 
		PendingRoomName.IsEmpty() ? TEXT("비어있음") : *PendingRoomName);
	
	UWorld* World = GetWorld();
	if (World)
	{
		ENetMode NetMode = World->GetNetMode();
		UE_LOG(LogTemp, Warning, TEXT("[GameInstance] OnStart 시점 NetMode: %s"), 
			NetMode == NM_Standalone ? TEXT("Standalone") :
			NetMode == NM_ListenServer ? TEXT("ListenServer") :
			NetMode == NM_Client ? TEXT("Client") :
			NetMode == NM_DedicatedServer ? TEXT("DedicatedServer") : TEXT("Unknown"));
		
		// PendingRoomName이 있고 Standalone 모드이면 자동으로 ListenServer 모드로 전환
		// (방 생성을 위해 서버가 필요하므로)
		// PendingRoomName이 없으면 Standalone 유지 (클라이언트는 나중에 서버 IP로 연결)
		if (NetMode == NM_Standalone && !PendingRoomName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Standalone 모드 + PendingRoomName 감지 - 자동으로 ListenServer 모드로 전환합니다."));
			
			// 현재 맵 경로 가져오기
			FString CurrentMapPath = UGameplayStatics::GetCurrentLevelName(World, true);
			if (CurrentMapPath.IsEmpty())
			{
				CurrentMapPath = World->GetMapName();
				CurrentMapPath.RemoveFromStart(World->StreamingLevelsPrefix);
			}
			
			// 맵 경로를 /Game/.../MapName.MapName 형식으로 변환
			if (!CurrentMapPath.Contains(TEXT("/")))
			{
				CurrentMapPath = FString::Printf(TEXT("/Game/Main/Level/%s.%s"), *CurrentMapPath, *CurrentMapPath);
			}
			else if (!CurrentMapPath.Contains(TEXT(".")))
			{
				FString MapName = FPaths::GetBaseFilename(CurrentMapPath);
				CurrentMapPath = FString::Printf(TEXT("%s.%s"), *CurrentMapPath, *MapName);
			}
			
			// 맵 경로가 유효한지 확인
			if (CurrentMapPath.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] 맵 경로를 가져올 수 없습니다. ListenServer 전환을 건너뜁니다."));
				return;
			}
			
			FString ListenURL = FString::Printf(TEXT("%s?listen"), *CurrentMapPath);
			FString OpenCommand = FString::Printf(TEXT("open %s"), *ListenURL);
			
			UE_LOG(LogTemp, Warning, TEXT("[GameInstance] ListenServer 모드로 전환 시도"));
			UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 명령어: %s"), *OpenCommand);
			
			// GEngine과 World가 유효한지 확인
			if (GEngine && World && IsValid(World))
			{
				FString DebugMsg = FString::Printf(
					TEXT("[GameInstance] Standalone 모드 + PendingRoomName 감지!\n")
					TEXT("자동으로 ListenServer 모드로 전환합니다.\n")
					TEXT("명령어: %s"),
					*OpenCommand
				);
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugMsg);
			}
			
			// World와 GEngine이 유효한지 확인
			if (!World || !IsValid(World) || !GEngine)
			{
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] World 또는 GEngine이 유효하지 않습니다. ListenServer 전환을 건너뜁니다."));
				return;
			}
			
			// 짧은 지연 후 실행 (World가 완전히 초기화될 시간 필요)
			// 람다에서 World를 캡처하지 않고 콜백 시점에 GetWorld()로 가져와 댕글링 포인터 크래시 방지
			// 멤버 핸들 사용: PIE 종료 시 Shutdown에서 명시적으로 클리어해 월드 참조 잔류/GC 실패 방지
			FString OpenCommandCopy = OpenCommand;
			World->GetTimerManager().SetTimer(ListenServerTimerHandle, [this, OpenCommandCopy]()
			{
				if (!IsValid(this) || !GEngine)
				{
					UE_LOG(LogTemp, Error, TEXT("[GameInstance] 타이머 콜백: GameInstance 또는 GEngine이 유효하지 않습니다."));
					return;
				}
				UWorld* CurrentWorld = GetWorld();
				if (!CurrentWorld || !IsValid(CurrentWorld))
				{
					UE_LOG(LogTemp, Error, TEXT("[GameInstance] 타이머 콜백: GetWorld()가 유효하지 않습니다."));
					return;
				}
				// GEngine->Exec 사용 (open 명령은 World 전환을 일으키므로 PC 대신 Exec이 안전)
				bool bExecResult = GEngine->Exec(CurrentWorld, *OpenCommandCopy);
				UE_LOG(LogTemp, Warning, TEXT("[GameInstance] ✅ ListenServer 전환 Exec 결과: %s, 명령: %s"), bExecResult ? TEXT("성공") : TEXT("실패"), *OpenCommandCopy);
			}, 0.1f, false); // 0.1초 후 실행
			
			// ListenServer로 전환되면 함수 종료 (아래 PendingRoomName 로직은 ListenServer 모드에서 실행됨)
			return;
		}
		else if (NetMode == NM_Standalone && PendingRoomName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Standalone 모드 유지 (클라이언트 모드 - 방 참가 대기 중)"));
		}
	}
	
	// PendingRoomName이 있으면 자동으로 세션 생성 시도 (ListenServer 모드에서)
	if (!PendingRoomName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameInstance] PendingRoomName 감지: %s"), *PendingRoomName);
		UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 자동으로 세션 생성을 시도합니다."));
		
		// 위에서 이미 World 변수를 선언했으므로 재사용
		if (World)
		{
			ENetMode NetMode = World->GetNetMode();
			UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 현재 NetMode: %s"), 
				NetMode == NM_Standalone ? TEXT("Standalone") :
				NetMode == NM_ListenServer ? TEXT("ListenServer") :
				NetMode == NM_Client ? TEXT("Client") :
				NetMode == NM_DedicatedServer ? TEXT("DedicatedServer") : TEXT("Unknown"));
			
			// Standalone 모드인 경우에만 ListenServer로 전환 시도
			if (NetMode == NM_Standalone)
			{
				// 현재 맵 경로 가져오기
				FString CurrentMapPath = UGameplayStatics::GetCurrentLevelName(World, true);
				if (CurrentMapPath.IsEmpty())
				{
					CurrentMapPath = World->GetMapName();
					CurrentMapPath.RemoveFromStart(World->StreamingLevelsPrefix);
				}
				
				// 맵 경로를 /Game/.../MapName.MapName 형식으로 변환
				if (!CurrentMapPath.Contains(TEXT("/")))
				{
					CurrentMapPath = FString::Printf(TEXT("/Game/Main/Level/%s.%s"), *CurrentMapPath, *CurrentMapPath);
				}
				else if (!CurrentMapPath.Contains(TEXT(".")))
				{
					FString MapName = FPaths::GetBaseFilename(CurrentMapPath);
					CurrentMapPath = FString::Printf(TEXT("%s.%s"), *CurrentMapPath, *MapName);
				}
				
				FString ListenURL = FString::Printf(TEXT("%s?listen"), *CurrentMapPath);
				FString OpenCommand = FString::Printf(TEXT("open %s"), *ListenURL);
				
				UE_LOG(LogTemp, Warning, TEXT("[GameInstance] ListenServer 모드로 전환 시도"));
				UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 명령어: %s"), *OpenCommand);
				
				if (GEngine)
				{
					FString DebugMsg = FString::Printf(
						TEXT("[GameInstance] PendingRoomName 감지!\n")
						TEXT("자동으로 ListenServer 모드로 전환합니다.\n")
						TEXT("명령어: %s")
					);
					GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, DebugMsg);
				}
				
				// PlayerController를 통한 ConsoleCommand 실행
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					PC->ConsoleCommand(OpenCommand, /*bExecInEditor=*/false);
					UE_LOG(LogTemp, Warning, TEXT("[GameInstance] ✅ ConsoleCommand 실행 완료: %s"), *OpenCommand);
				}
				else
				{
					// PlayerController가 없으면 GEngine->Exec 사용
					if (GEngine)
					{
						bool bExecResult = GEngine->Exec(World, *OpenCommand);
						UE_LOG(LogTemp, Warning, TEXT("[GameInstance] GEngine->Exec 결과: %s"), bExecResult ? TEXT("성공") : TEXT("실패"));
					}
				}
			}
			else if (NetMode == NM_ListenServer)
			{
				UE_LOG(LogTemp, Warning, TEXT("[GameInstance] ✅ 이미 ListenServer 모드입니다!"));
				UE_LOG(LogTemp, Warning, TEXT("[GameInstance] PendingRoomName으로 세션을 다시 생성합니다: %s"), *PendingRoomName);
				
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, 
						TEXT("[GameInstance] ✅ 이미 ListenServer 모드입니다! 세션을 다시 생성합니다."));
				}
				
				// ListenServer 모드가 되었으므로 세션을 다시 생성
				// GameSession이 초기화될 때까지 여러 번 시도
				// 멤버 핸들 사용: PIE 종료 시 Shutdown에서 명시적으로 클리어해 월드 참조 잔류/GC 실패 방지
				int32 RetryCount = 0;
				const int32 MaxRetries = 10; // 최대 10초 대기
				
				TWeakObjectPtr<UWorld> WeakWorld = World;
				FString RoomNameToCreate = PendingRoomName; // 복사본 저장
				
				UE_LOG(LogTemp, Error, TEXT("[GameInstance] ⚠️ GameSession 찾기 시작 (최대 %d초 대기)"), MaxRetries);
				
				World->GetTimerManager().SetTimer(SessionRecreateTimerHandle, [this, WeakWorld, RoomNameToCreate, RetryCount, MaxRetries]() mutable
				{
					int32 CurrentRetry = RetryCount;
					CurrentRetry++;
					
					if (!WeakWorld.IsValid())
					{
						UE_LOG(LogTemp, Error, TEXT("[GameInstance] ❌ World가 유효하지 않습니다."));
						return;
					}
					
					UWorld* W = WeakWorld.Get();
					if (!W)
					{
						UE_LOG(LogTemp, Error, TEXT("[GameInstance] ❌ World 포인터가 NULL입니다."));
						return;
					}
					
					UE_LOG(LogTemp, Error, TEXT("[GameInstance] GameMode 찾기 시도 %d/%d"), CurrentRetry, MaxRetries);
					
					AGameModeBase* GameMode = W->GetAuthGameMode();
					if (!GameMode)
					{
						UE_LOG(LogTemp, Error, TEXT("[GameInstance] ❌ GameMode를 찾을 수 없습니다. 재시도 중... (%d/%d)"), CurrentRetry, MaxRetries);
						if (CurrentRetry >= MaxRetries)
						{
							UE_LOG(LogTemp, Error, TEXT("[GameInstance] ❌ GameMode를 찾을 수 없습니다. 최대 재시도 횟수 초과."));
							UE_LOG(LogTemp, Error, TEXT("[GameInstance] ⚠️ 블루프린트 GameMode(BP_MainGameMode)가 C++ ABRGameMode를 상속하는지 확인하세요."));
							return;
						}
						return; // 타이머가 계속 실행됨
					}
					
					UE_LOG(LogTemp, Error, TEXT("[GameInstance] ✅ GameMode 발견: %s"), *GameMode->GetClass()->GetName());
					
					ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession);
					if (!GameSession)
					{
						UE_LOG(LogTemp, Error, TEXT("[GameInstance] ❌ GameSession을 찾을 수 없습니다. 재시도 중... (%d/%d)"), CurrentRetry, MaxRetries);
						if (GameMode->GameSession)
						{
							UE_LOG(LogTemp, Error, TEXT("[GameInstance] GameMode->GameSession 타입: %s (ABRGameSession이 아님)"), 
								*GameMode->GameSession->GetClass()->GetName());
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("[GameInstance] GameMode->GameSession이 NULL입니다."));
						}
						if (CurrentRetry >= MaxRetries)
						{
							UE_LOG(LogTemp, Error, TEXT("[GameInstance] ❌ GameSession을 찾을 수 없습니다. 최대 재시도 횟수 초과."));
							UE_LOG(LogTemp, Error, TEXT("[GameInstance] ⚠️ 블루프린트 GameMode에서 GameSessionClass가 ABRGameSession으로 설정되어 있는지 확인하세요."));
							return;
						}
						return; // 타이머가 계속 실행됨
					}
					
					// GameSession을 찾았으므로 세션 생성
					UE_LOG(LogTemp, Error, TEXT("[GameInstance] ✅✅✅ GameSession 발견! ListenServer 모드에서 세션 재생성: %s"), *RoomNameToCreate);
					GameSession->CreateRoomSession(RoomNameToCreate);
					
					// PendingRoomName 클리어 (재생성 완료)
					if (UBRGameInstance* BRGI = Cast<UBRGameInstance>(W->GetGameInstance()))
					{
						BRGI->ClearPendingRoomName();
					}
					
					// 타이머 정리
					W->GetTimerManager().ClearTimer(SessionRecreateTimerHandle);
				}, 1.0f, true); // 1초마다 반복
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameInstance] World가 없습니다. ListenServer 전환을 시도할 수 없습니다."));
		}
	}
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

void UBRGameInstance::SetLANOnly(int32 bEnabled)
{
	bUseLANOnly = (bEnabled != 0);
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] SetLANOnly: %s (%s)"), 
		bUseLANOnly ? TEXT("1") : TEXT("0"),
		bUseLANOnly ? TEXT("LAN 전용") : TEXT("인터넷 매칭"));
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

/** [핵심] JSON 데이터를 읽어 DT를 갱신하고 에셋으로 저장함 */
void UBRGameInstance::ReloadAllConfigs()
{
	GI_LOG(Display, TEXT("=== Starting Global Config Reload and Asset Sync ==="));

	if (ConfigDataMap.Num() == 0)
	{
		GI_LOG(Warning, TEXT("ConfigDataMap이 비어 있습니다. 에디터에서 설정이 필요합니다."));
		return;
	}

	for (auto& Elem : ConfigDataMap)
	{
		const FString& JsonFileName = Elem.Key;
		UDataTable* TargetTable = Elem.Value;

		if (TargetTable)
		{
			// 1. JSON 파일 읽어서 메모리상 DT 업데이트
			UpdateDataTableFromJson(TargetTable, JsonFileName);

			// 2. 에디터 환경이고 게임이 실행 중이 아닌 경우에만 .uasset 파일로 영구 저장
			// Standalone 모드나 PIE 모드에서는 저장하지 않음
		#if WITH_EDITOR
			// 에디터에서만 저장하고, 게임 실행 중이 아닐 때만 저장
			// GetWorld()가 있으면 게임이 실행 중인 것으로 간주
			if (GIsEditor && !GetWorld())
			{
				SaveDataTableToAsset(TargetTable);
			}
		#endif
		}
	}
	
	// 글로벌 배율 적용 로직 호출
	ApplyGlobalMultipliers();

	// 3. 월드에 이미 존재하는 무기들에게 최신 데이터를 적용 (기존 로직 유지)
	if (GetWorld())
	{
		for (TActorIterator<ABaseWeapon> It(GetWorld()); It; ++It)
		{
			It->LoadWeaponData();
		}
	}

	GI_LOG(Display, TEXT("=== Global Config Reload Complete ==="));
}

void UBRGameInstance::LoadConfigFromJson(const FString& FileName, UDataTable* TargetTable)
{
	if (!TargetTable) return;

	// 경로: 프로젝트/Config/파일명.json
	FString FilePath = GetConfigDirectory() + FileName + TEXT(".json");
	FString JsonString;

	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		GI_LOG(Warning, TEXT("File not found: %s"), *FilePath);
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* DataArray;
		// 모든 JSON의 최상위 배열 키를 "Data"로 통일하거나 파일명과 맞춥니다.
		if (RootObject->TryGetArrayField(TEXT("Data"), DataArray))
		{
			// const UScriptStruct* 로 선언하여 타입 에러 해결
			const UScriptStruct* TableStruct = TargetTable->GetRowStruct();

			for (const auto& Value : *DataArray)
			{
				TSharedPtr<FJsonObject> DataObj = Value->AsObject();
				if (!DataObj.IsValid()) continue;

				FName RowID = FName(*DataObj->GetStringField(TEXT("Name")));
				uint8* RowPtr = TargetTable->FindRowUnchecked(RowID);

				if (RowPtr && TableStruct)
				{
					// 수치 데이터 주입
					FJsonObjectConverter::JsonObjectToUStruct(DataObj.ToSharedRef(), TableStruct, RowPtr);
					GI_LOG(Log, TEXT("[%s.json] Row Updated: %s"), *FileName, *RowID.ToString());
				}
			}
		}
	}
}

FString UBRGameInstance::GetConfigDirectory()
{
	FString TargetPath;

#if WITH_EDITOR
	// 1. 에디터 환경: 프로젝트 루트의 Data 폴더
	TargetPath = FPaths::ProjectDir() / TEXT("Data/");
#else
	// 2. 패키징 환경: 빌드된 .exe 옆의 Data 폴더 (예: Build/Windows/MyProject/Data/)
	// FPaths::ProjectDir()는 패키징 후에도 실행 파일 기준 경로를 반환합니다.
	TargetPath = FPaths::ProjectDir() / TEXT("Data/");
#endif

	return TargetPath;
}

/** JSON 문자열을 DataTable에 주입 */
void UBRGameInstance::UpdateDataTableFromJson(UDataTable* TargetTable, FString FileName)
{
	if (!TargetTable) return;

	FString FullPath = GetConfigDirectory() + FileName + TEXT(".json");
	FString JsonString;

	if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
	{
		GI_LOG(Warning, TEXT("JSON 파일을 찾을 수 없습니다: %s"), *FullPath);
		return;
	}

	// 1. JSON 파싱
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* DataArray;
		// JSON의 "Data" 배열 필드를 가져옴
		if (RootObject->TryGetArrayField(TEXT("Data"), DataArray))
		{
			const UScriptStruct* TableStruct = TargetTable->GetRowStruct();

			for (const auto& Value : *DataArray)
			{
				TSharedPtr<FJsonObject> DataObj = Value->AsObject();
				if (!DataObj.IsValid()) continue;

				// 1. 행 이름(Name) 확인
				FString NameStr = DataObj->GetStringField(TEXT("Name"));
				if (NameStr.IsEmpty()) continue;

				FName RowName = FName(*NameStr);

				// 2. 기존 행 찾기
				uint8* RowPtr = TargetTable->FindRowUnchecked(RowName);

				// 3. 행이 없으면 새로 추가
				if (!RowPtr)
				{
					// 빈 데이터 구조체를 생성하여 테이블에 추가
					TargetTable->AddRow(RowName, FTableRowBase());
					// 추가된 행의 포인터를 다시 가져옴
					RowPtr = TargetTable->FindRowUnchecked(RowName);

					GI_LOG(Log, TEXT("[%s] 새로운 행 생성됨: %s"), *FileName, *RowName.ToString());
				}

				// 4. 데이터 주입 (기본적으로 기존 데이터는 유지하고 JSON에 있는 필드만 덮어씀)
				if (RowPtr && TableStruct)
				{
					FJsonObjectConverter::JsonObjectToUStruct(DataObj.ToSharedRef(), TableStruct, RowPtr);
					GI_LOG(Log, TEXT("[%s] 데이터 업데이트 완료: %s"), *FileName, *RowName.ToString());
				}
			}

			// 데이터 테이블 구조 갱신 알림 (에디터 UI 등에 즉시 반영)
			TargetTable->Modify();

			#if WITH_EDITOR
			// 에디터에게 데이터 테이블의 구조나 내용이 바뀌었음을 알림
			// 이 함수는 UDataTable에 정의되어 있으며, 에디터 UI를 즉시 새로고침합니다.
				TargetTable->OnDataTableChanged().Broadcast();

			// (선택 사항) 데이터 테이블 에셋 아이콘에 별표(*) 표시 (수정됨 표시)
				TargetTable->PostEditChange();
			#endif
		}
	}
}


/** 에셋 파일(.uasset)로 영구 저장 */
void UBRGameInstance::SaveDataTableToAsset(UDataTable* TargetTable)
{
#if WITH_EDITOR
	// 게임 실행 중이면 저장하지 않음 (Standalone, PIE 모드 등)
	// GetWorld()가 있으면 게임이 실행 중인 것으로 간주
	if (GetWorld())
	{
		GI_LOG(Warning, TEXT("게임 실행 중이므로 Asset 저장을 건너뜁니다."));
		return;
	}

	if (!TargetTable) 
	{
		GI_LOG(Error, TEXT("TargetTable이 유효하지 않습니다."));
		return;
	}

	UPackage* Package = TargetTable->GetOutermost();
	if (!Package) 
	{
		GI_LOG(Error, TEXT("Package를 찾을 수 없습니다."));
		return;
	}

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		Package->GetName(),
		FPackageName::GetAssetPackageExtension()
	);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GError;
	SaveArgs.bForceByteSwapping = true;

	if (UPackage::SavePackage(Package, TargetTable, *PackageFileName, SaveArgs))
	{
		GI_LOG(Log, TEXT("Asset 영구 저장 성공: %s"), *PackageFileName);
	}
	else
	{
		GI_LOG(Error, TEXT("Asset 저장 실패: %s"), *PackageFileName);
	}
#else
	// 에디터가 아닌 환경에서는 저장하지 않음
	GI_LOG(Warning, TEXT("에디터가 아니므로 Asset 저장을 건너뜁니다."));
#endif
}

// BRGameInstance.cpp 의 ApplyGlobalMultipliers 함수 수정
#include "StaminaComponent.h" // 헤더 추가 필수

void UBRGameInstance::ApplyGlobalMultipliers()
{
	if (UDataTable** TargetTablePtr = ConfigDataMap.Find(TEXT("GlobalSettings")))
	{
		UDataTable* GlobalTable = *TargetTablePtr;
		if (GlobalTable)
		{
			static const FString ContextString(TEXT("Global Settings Context"));
			FGlobalBalanceData* FoundData = GlobalTable->FindRow<FGlobalBalanceData>(FName("Default"), ContextString);

			if (FoundData)
			{
				// 1. 무기 관련 전역 변수 업데이트
				ABaseWeapon::GlobalDamageMultiplier = FoundData->Global_Weapon_DamageMultiplier;
				ABaseWeapon::GlobalImpulseMultiplier = FoundData->Global_Weapon_ImpulseMultiplier;
				ABaseWeapon::GlobalAttackSpeedMultiplier = FoundData->Global_Weapon_AttackSpeedMultiplier;

				// 2. 스태미나 관련 전역(static) 변수 업데이트
				UStaminaComponent::Global_SprintDrainRate = FoundData->Global_Stamina_SprintDrainRate;
				UStaminaComponent::Global_JumpCost = FoundData->Global_Stamina_JumpCost;
				UStaminaComponent::Global_RegenRate = FoundData->Global_Stamina_RegenRate;

				// 3. (핵심) 이미 소환된 캐릭터들에게도 즉시 적용 (실시간 리로드를 위해)
				if (UWorld* World = GetWorld())
				{
					for (TActorIterator<APlayerCharacter> It(World); It; ++It)
					{
						if (UStaminaComponent* StaminaComp = It->StaminaComp)
						{
							StaminaComp->StaminaDrainRate = UStaminaComponent::Global_SprintDrainRate;
							StaminaComp->JumpCost = UStaminaComponent::Global_JumpCost;
							StaminaComp->StaminaRegenRate = UStaminaComponent::Global_RegenRate;
						}
					}
				}

				GI_LOG(Display, TEXT("스태미나 세팅 적용. Stamina: Drain(%.1f), Jump(%.1f), Regen(%.1f)"),
					UStaminaComponent::Global_SprintDrainRate,
					UStaminaComponent::Global_JumpCost,
					UStaminaComponent::Global_RegenRate);

				GI_LOG(Display, TEXT("무기 배율 세팅 적용. Weapon: Damage(%.1f), Impulse(%.1f), AttackSpeed(%.1f)"),
					ABaseWeapon::GlobalDamageMultiplier,
					ABaseWeapon::GlobalImpulseMultiplier,
					ABaseWeapon::GlobalAttackSpeedMultiplier);
			}
		}
	}
}

void UBRGameInstance::DoPIEExitCleanup(UWorld* World)
{
	if (!World || !World->IsPlayInEditor())
	{
		return;
	}
	GI_LOG(Warning, TEXT("PIE 종료 정리(DoPIEExitCleanup) - World 참조 사슬 해제"));
	
	// SessionInterface→GameSession→World 참조 끊기
	if (AGameModeBase* GameMode = World->GetAuthGameMode())
	{
		if (ABRGameSession* GameSession = Cast<ABRGameSession>(GameMode->GameSession))
		{
			GameSession->UnbindSessionDelegatesForPIEExit();
		}
	}
	
	World->GetTimerManager().ClearTimer(ListenServerTimerHandle);
	World->GetTimerManager().ClearTimer(SessionRecreateTimerHandle);
	World->GetTimerManager().ClearAllTimersForObject(this);
	
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		NavSys->CleanUp();
	}
}

void UBRGameInstance::Shutdown()
{
	// OnWorldCleanup은 해제하지 않음(FDelegateHandle/헤더 경로 이슈 회피). Shutdown 시점에 한 번 더 정리.
	UWorld* World = GetWorld();
	if (World && World->IsPlayInEditor())
	{
		DoPIEExitCleanup(World);
	}
	
	Super::Shutdown();
}