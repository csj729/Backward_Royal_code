// BRGameInstance.cpp
#include "BRGameInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BRGameMode.h"
#include "BRGameSession.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "BRPlayerState.h"
#include "BaseWeapon.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GlobalBalanceData.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "PlayerCharacter.h"
#include "StaminaComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "UObject/SavePackage.h"
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY(LogBRGameInstance);

#define GI_LOG(Verbosity, Format, ...)                                         \
  UE_LOG(LogBRGameInstance, Verbosity, TEXT("%s: ") Format,                    \
         *FString(__FUNCTION__), ##__VA_ARGS__)

UBRGameInstance::UBRGameInstance() {}

void UBRGameInstance::Init() {
  Super::Init();
  UE_LOG(
      LogTemp, Log,
      TEXT(
          "[GameInstance] BRGameInstance 초기화 완료 - 콘솔 명령어 사용 가능"));
  UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 네트워크 모드: %s"),
         bUseLANOnly ? TEXT("LAN 전용") : TEXT("인터넷 매칭 (Steam)"));
  UE_LOG(LogTemp, Warning,
         TEXT("[GameInstance] 모드 변경: 콘솔에서 'SetLANOnly 1' (LAN) 또는 "
              "'SetLANOnly 0' (인터넷)"));

  // S_UserInfo 에셋에서 PlayerName 로드
  LoadPlayerNameFromUserInfo();

  // PIE 월드 클린업이 엔진의 '월드 참조 검사'보다 먼저 일어나게 등록.
  // Shutdown에서 Remove.
  TWeakObjectPtr<UBRGameInstance> Self(this);
  OnWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda(
      [Self](UWorld *InWorld, bool bSessionEnding, bool bCleanupResources) {
        if (InWorld && InWorld->IsPlayInEditor() && Self.IsValid() &&
            InWorld->GetGameInstance() == Self.Get()) {
          Self->DoPIEExitCleanup(InWorld);
        }
      });

#if WITH_EDITOR
  // PrePIEEnded는 PIE 종료 시 엔진의 참조 검사(AddReferencedObjects)보다 먼저 호출됨.
  // 여기서 월드 참조를 끊어주어야 GC Assertion을 방지할 수 있음.
  PrePIEEndedHandle = FEditorDelegates::PrePIEEnded.AddLambda([Self](bool bIsSimulating) {
	  if (Self.IsValid())
	  {
		  if (UWorld* MyWorld = Self->GetWorld())
		  {
			  if (MyWorld->IsPlayInEditor())
			  {
				  Self->DoPIEExitCleanup(MyWorld);
			  }
		  }
	  }
  });
#endif

  // 패킹된 게임에서 Standalone 모드로 시작하는 것을 방지하기 위해
  // 명령줄 인자 확인 (이미 ?listen이 있으면 그대로 사용)
  FString CommandLine = FCommandLine::Get();
  UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 명령줄: %s"), *CommandLine);

  ReloadAllConfigs();
}

void UBRGameInstance::OnStart() {
  Super::OnStart();

  UWorld *World = GetWorld();
  if (!World)
    return;

  ENetMode NetMode = World->GetNetMode();

  // 방 생성 흐름(복구)이 아닐 때는 로그 생략 — 일반 시작에서는 OnStart 로그
  // 없음
  if (PendingRoomName.IsEmpty()) {
    // 상세 로그는 아래 PendingRoomName 분기(방 생성 복구)에서만 출력
  } else {
    UE_LOG(
        LogTemp, Warning,
        TEXT("[GameInstance] OnStart — PendingRoomName 감지(방 생성 복구): %s"),
        *PendingRoomName);
    UE_LOG(LogTemp, Warning, TEXT("[GameInstance] OnStart 시점 NetMode: %s"),
           NetMode == NM_Standalone        ? TEXT("Standalone")
           : NetMode == NM_ListenServer    ? TEXT("ListenServer")
           : NetMode == NM_Client          ? TEXT("Client")
           : NetMode == NM_DedicatedServer ? TEXT("DedicatedServer")
                                           : TEXT("Unknown"));
  }

  if (World) {
    // PendingRoomName이 있고 Standalone 모드이면 자동으로 ListenServer 모드로
    // 전환 (방 생성을 위해 서버가 필요하므로) PendingRoomName이 없으면
    // Standalone 유지 (클라이언트는 나중에 서버 IP로 연결)
    if (NetMode == NM_Standalone && !PendingRoomName.IsEmpty()) {
      UE_LOG(LogTemp, Warning,
             TEXT("[GameInstance] Standalone 모드 + PendingRoomName 감지 - "
                  "자동으로 ListenServer 모드로 전환합니다."));

      // 현재 맵 경로 가져오기
      FString CurrentMapPath =
          UGameplayStatics::GetCurrentLevelName(World, true);
      if (CurrentMapPath.IsEmpty()) {
        CurrentMapPath = World->GetMapName();
        CurrentMapPath.RemoveFromStart(World->StreamingLevelsPrefix);
      }

      // 맵 경로를 /Game/.../MapName.MapName 형식으로 변환
      if (!CurrentMapPath.Contains(TEXT("/"))) {
        CurrentMapPath = FString::Printf(TEXT("/Game/Main/Level/%s.%s"),
                                         *CurrentMapPath, *CurrentMapPath);
      } else if (!CurrentMapPath.Contains(TEXT("."))) {
        FString MapName = FPaths::GetBaseFilename(CurrentMapPath);
        CurrentMapPath =
            FString::Printf(TEXT("%s.%s"), *CurrentMapPath, *MapName);
      }

      // 맵 경로가 유효한지 확인
      if (CurrentMapPath.IsEmpty()) {
        UE_LOG(LogTemp, Error,
               TEXT("[GameInstance] 맵 경로를 가져올 수 없습니다. ListenServer "
                    "전환을 건너뜁니다."));
        return;
      }

      FString ListenURL = FString::Printf(TEXT("%s?listen"), *CurrentMapPath);
      FString OpenCommand = FString::Printf(TEXT("open %s"), *ListenURL);

      UE_LOG(LogTemp, Warning,
             TEXT("[GameInstance] ListenServer 모드로 전환 시도"));
      UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 명령어: %s"), *OpenCommand);

      // GEngine과 World가 유효한지 확인
      if (GEngine && World && IsValid(World)) {
        FString DebugMsg = FString::Printf(
            TEXT("[GameInstance] Standalone 모드 + PendingRoomName 감지!\n")
                TEXT("자동으로 ListenServer 모드로 전환합니다.\n")
                    TEXT("명령어: %s"),
            *OpenCommand);
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugMsg);
      }

      // World와 GEngine이 유효한지 확인
      if (!World || !IsValid(World) || !GEngine) {
        UE_LOG(LogTemp, Error,
               TEXT("[GameInstance] World 또는 GEngine이 유효하지 않습니다. "
                    "ListenServer 전환을 건너뜁니다."));
        return;
      }

      // 짧은 지연 후 실행 (World가 완전히 초기화될 시간 필요)
      // 람다에서 World를 캡처하지 않고 콜백 시점에 GetWorld()로 가져와 댕글링
      // 포인터 크래시 방지 멤버 핸들 사용: PIE 종료 시 Shutdown에서 명시적으로
      // 클리어해 월드 참조 잔류/GC 실패 방지
      FString OpenCommandCopy = OpenCommand;
      World->GetTimerManager().SetTimer(
          ListenServerTimerHandle,
          [this, OpenCommandCopy]() {
            if (!IsValid(this) || !GEngine) {
              UE_LOG(LogTemp, Error,
                     TEXT("[GameInstance] 타이머 콜백: GameInstance 또는 "
                          "GEngine이 유효하지 않습니다."));
              return;
            }
            UWorld *CurrentWorld = GetWorld();
            if (!CurrentWorld || !IsValid(CurrentWorld)) {
              UE_LOG(LogTemp, Error,
                     TEXT("[GameInstance] 타이머 콜백: GetWorld()가 유효하지 "
                          "않습니다."));
              return;
            }
            // GEngine->Exec 사용 (open 명령은 World 전환을 일으키므로 PC 대신
            // Exec이 안전)
            bool bExecResult = GEngine->Exec(CurrentWorld, *OpenCommandCopy);
            UE_LOG(LogTemp, Warning,
                   TEXT("[GameInstance] ✅ ListenServer 전환 Exec 결과: %s, "
                        "명령: %s"),
                   bExecResult ? TEXT("성공") : TEXT("실패"), *OpenCommandCopy);
          },
          0.1f, false); // 0.1초 후 실행

      // ListenServer로 전환되면 함수 종료 (아래 PendingRoomName 로직은
      // ListenServer 모드에서 실행됨)
      return;
    }
    // Standalone + PendingRoomName 비어있음 → 위에서 이미 한 줄 로그로 처리,
    // 중복 로그 생략
  }

  // PendingRoomName이 있으면 자동으로 세션 생성 시도 (ListenServer 모드에서)
  if (!PendingRoomName.IsEmpty()) {
    UE_LOG(LogTemp, Warning, TEXT("[GameInstance] PendingRoomName 감지: %s"),
           *PendingRoomName);
    UE_LOG(LogTemp, Warning,
           TEXT("[GameInstance] 자동으로 세션 생성을 시도합니다."));

    // 위에서 이미 World 변수를 선언했으므로 재사용
    if (World) {
      NetMode = World->GetNetMode();
      UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 현재 NetMode: %s"),
             NetMode == NM_Standalone        ? TEXT("Standalone")
             : NetMode == NM_ListenServer    ? TEXT("ListenServer")
             : NetMode == NM_Client          ? TEXT("Client")
             : NetMode == NM_DedicatedServer ? TEXT("DedicatedServer")
                                             : TEXT("Unknown"));

      // Standalone 모드인 경우에만 ListenServer로 전환 시도
      if (NetMode == NM_Standalone) {
        // 현재 맵 경로 가져오기
        FString CurrentMapPath =
            UGameplayStatics::GetCurrentLevelName(World, true);
        if (CurrentMapPath.IsEmpty()) {
          CurrentMapPath = World->GetMapName();
          CurrentMapPath.RemoveFromStart(World->StreamingLevelsPrefix);
        }

        // 맵 경로를 /Game/.../MapName.MapName 형식으로 변환
        if (!CurrentMapPath.Contains(TEXT("/"))) {
          CurrentMapPath = FString::Printf(TEXT("/Game/Main/Level/%s.%s"),
                                           *CurrentMapPath, *CurrentMapPath);
        } else if (!CurrentMapPath.Contains(TEXT("."))) {
          FString MapName = FPaths::GetBaseFilename(CurrentMapPath);
          CurrentMapPath =
              FString::Printf(TEXT("%s.%s"), *CurrentMapPath, *MapName);
        }

        FString ListenURL = FString::Printf(TEXT("%s?listen"), *CurrentMapPath);
        FString OpenCommand = FString::Printf(TEXT("open %s"), *ListenURL);

        UE_LOG(LogTemp, Warning,
               TEXT("[GameInstance] ListenServer 모드로 전환 시도"));
        UE_LOG(LogTemp, Warning, TEXT("[GameInstance] 명령어: %s"),
               *OpenCommand);

        if (GEngine) {
          FString DebugMsg = FString::Printf(
              TEXT("[GameInstance] PendingRoomName 감지!\n")
                  TEXT("자동으로 ListenServer 모드로 전환합니다.\n")
                      TEXT("명령어: %s"));
          GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, DebugMsg);
        }

        // PlayerController를 통한 ConsoleCommand 실행
        if (APlayerController *PC = World->GetFirstPlayerController()) {
          PC->ConsoleCommand(OpenCommand, /*bExecInEditor=*/false);
          UE_LOG(LogTemp, Warning,
                 TEXT("[GameInstance] ✅ ConsoleCommand 실행 완료: %s"),
                 *OpenCommand);
        } else {
          // PlayerController가 없으면 GEngine->Exec 사용
          if (GEngine) {
            bool bExecResult = GEngine->Exec(World, *OpenCommand);
            UE_LOG(LogTemp, Warning,
                   TEXT("[GameInstance] GEngine->Exec 결과: %s"),
                   bExecResult ? TEXT("성공") : TEXT("실패"));
          }
        }
      } else if (NetMode == NM_ListenServer) {
        UE_LOG(LogTemp, Warning,
               TEXT("[GameInstance] ✅ 이미 ListenServer 모드입니다!"));
        UE_LOG(LogTemp, Warning,
               TEXT("[GameInstance] PendingRoomName으로 세션을 다시 "
                    "생성합니다: %s"),
               *PendingRoomName);

        if (GEngine) {
          GEngine->AddOnScreenDebugMessage(
              -1, 5.0f, FColor::Green,
              TEXT("[GameInstance] ✅ 이미 ListenServer 모드입니다! 세션을 "
                   "다시 생성합니다."));
        }

        // ListenServer 모드가 되었으므로 세션을 다시 생성
        // GameSession이 초기화될 때까지 여러 번 시도
        // 멤버 핸들 사용: PIE 종료 시 Shutdown에서 명시적으로 클리어해 월드
        // 참조 잔류/GC 실패 방지
        int32 RetryCount = 0;
        const int32 MaxRetries = 10; // 최대 10초 대기

        TWeakObjectPtr<UWorld> WeakWorld = World;
        FString RoomNameToCreate = PendingRoomName; // 복사본 저장

        UE_LOG(LogTemp, Error,
               TEXT("[GameInstance] ⚠️ GameSession 찾기 시작 (최대 %d초 대기)"),
               MaxRetries);

        World->GetTimerManager().SetTimer(
            SessionRecreateTimerHandle,
            [this, WeakWorld, RoomNameToCreate, RetryCount,
             MaxRetries]() mutable {
              int32 CurrentRetry = RetryCount;
              CurrentRetry++;

              if (!WeakWorld.IsValid()) {
                UE_LOG(LogTemp, Error,
                       TEXT("[GameInstance] ❌ World가 유효하지 않습니다."));
                return;
              }

              UWorld *W = WeakWorld.Get();
              if (!W) {
                UE_LOG(LogTemp, Error,
                       TEXT("[GameInstance] ❌ World 포인터가 NULL입니다."));
                return;
              }

              UE_LOG(LogTemp, Error,
                     TEXT("[GameInstance] GameMode 찾기 시도 %d/%d"),
                     CurrentRetry, MaxRetries);

              AGameModeBase *GameMode = W->GetAuthGameMode();
              if (!GameMode) {
                UE_LOG(LogTemp, Error,
                       TEXT("[GameInstance] ❌ GameMode를 찾을 수 없습니다. "
                            "재시도 중... (%d/%d)"),
                       CurrentRetry, MaxRetries);
                if (CurrentRetry >= MaxRetries) {
                  UE_LOG(LogTemp, Error,
                         TEXT("[GameInstance] ❌ GameMode를 찾을 수 없습니다. "
                              "최대 재시도 횟수 초과."));
                  UE_LOG(LogTemp, Error,
                         TEXT("[GameInstance] ⚠️ 블루프린트 "
                              "GameMode(BP_MainGameMode)가 C++ ABRGameMode를 "
                              "상속하는지 확인하세요."));
                  return;
                }
                return; // 타이머가 계속 실행됨
              }

              UE_LOG(LogTemp, Error,
                     TEXT("[GameInstance] ✅ GameMode 발견: %s"),
                     *GameMode->GetClass()->GetName());

              ABRGameSession *GameSession =
                  Cast<ABRGameSession>(GameMode->GameSession);
              if (!GameSession) {
                UE_LOG(LogTemp, Error,
                       TEXT("[GameInstance] ❌ GameSession을 찾을 수 없습니다. "
                            "재시도 중... (%d/%d)"),
                       CurrentRetry, MaxRetries);
                if (GameMode->GameSession) {
                  UE_LOG(LogTemp, Error,
                         TEXT("[GameInstance] GameMode->GameSession 타입: %s "
                              "(ABRGameSession이 아님)"),
                         *GameMode->GameSession->GetClass()->GetName());
                } else {
                  UE_LOG(LogTemp, Error,
                         TEXT("[GameInstance] GameMode->GameSession이 "
                              "NULL입니다."));
                }
                if (CurrentRetry >= MaxRetries) {
                  UE_LOG(LogTemp, Error,
                         TEXT("[GameInstance] ❌ GameSession을 찾을 수 "
                              "없습니다. 최대 재시도 횟수 초과."));
                  UE_LOG(LogTemp, Error,
                         TEXT("[GameInstance] ⚠️ 블루프린트 GameMode에서 "
                              "GameSessionClass가 ABRGameSession으로 설정되어 "
                              "있는지 확인하세요."));
                  return;
                }
                return; // 타이머가 계속 실행됨
              }

              // GameSession을 찾았으므로 세션 생성
              UE_LOG(LogTemp, Error,
                     TEXT("[GameInstance] ✅✅✅ GameSession 발견! "
                          "ListenServer 모드에서 세션 재생성: %s"),
                     *RoomNameToCreate);
              GameSession->CreateRoomSession(RoomNameToCreate);

              // PendingRoomName 클리어 (재생성 완료)
              if (UBRGameInstance *BRGI =
                      Cast<UBRGameInstance>(W->GetGameInstance())) {
                BRGI->ClearPendingRoomName();
              }

              // 타이머 정리
              W->GetTimerManager().ClearTimer(SessionRecreateTimerHandle);
            },
            1.0f, true); // 1초마다 반복
      }
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[GameInstance] World가 없습니다. ListenServer 전환을 시도할 "
                  "수 없습니다."));
    }
  }
}

void UBRGameInstance::CreateRoom(const FString &RoomName) {
  UE_LOG(LogTemp, Log, TEXT("[GameInstance] CreateRoom 명령 실행: %s"),
         *RoomName);

  if (!GetWorld()) {
    UE_LOG(LogTemp, Error,
           TEXT("[GameInstance] World가 없습니다. 게임을 먼저 시작해주세요."));
    return;
  }

  // PlayerController를 통한 방법
  if (APlayerController *PC = GetWorld()->GetFirstPlayerController()) {
    if (ABRPlayerController *BRPC = Cast<ABRPlayerController>(PC)) {
      UE_LOG(LogTemp, Log,
             TEXT("[GameInstance] PlayerController를 통해 방 생성 요청"));
      BRPC->CreateRoom(RoomName);
      return;
    }
  }

  // GameMode를 통한 직접 접근 방법 (게임이 시작되지 않았을 때)
  if (AGameModeBase *GameMode = GetWorld()->GetAuthGameMode()) {
    if (ABRGameSession *GameSession =
            Cast<ABRGameSession>(GameMode->GameSession)) {
      UE_LOG(LogTemp, Log,
             TEXT("[GameInstance] GameSession을 통해 직접 방 생성 요청"));
      GameSession->CreateRoomSession(RoomName);
      return;
    }
  }

  UE_LOG(LogTemp, Error,
         TEXT("[GameInstance] 방 생성을 위한 필요한 객체를 찾을 수 없습니다."));
  UE_LOG(LogTemp, Warning,
         TEXT("[GameInstance] 게임을 시작한 후 다시 시도해주세요."));
}

void UBRGameInstance::SetLANOnly(int32 bEnabled) {
  bUseLANOnly = (bEnabled != 0);
  UE_LOG(LogTemp, Warning, TEXT("[GameInstance] SetLANOnly: %s (%s)"),
         bUseLANOnly ? TEXT("1") : TEXT("0"),
         bUseLANOnly ? TEXT("LAN 전용") : TEXT("인터넷 매칭"));
}

void UBRGameInstance::FindRooms() {
  UE_LOG(LogTemp, Log, TEXT("[GameInstance] FindRooms 명령"));

  if (UWorld *World = GetWorld()) {
    if (APlayerController *PC = World->GetFirstPlayerController()) {
      if (ABRPlayerController *BRPC = Cast<ABRPlayerController>(PC)) {
        BRPC->FindRooms();
      } else {
        UE_LOG(LogTemp, Error,
               TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
      }
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 "
                  "시작되지 않았을 수 있습니다."));
    }
  }
}

void UBRGameInstance::ToggleReady() {
  UE_LOG(LogTemp, Log, TEXT("[GameInstance] ToggleReady 명령"));

  if (UWorld *World = GetWorld()) {
    if (APlayerController *PC = World->GetFirstPlayerController()) {
      if (ABRPlayerController *BRPC = Cast<ABRPlayerController>(PC)) {
        BRPC->ToggleReady();
      } else {
        UE_LOG(LogTemp, Error,
               TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
      }
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 "
                  "시작되지 않았을 수 있습니다."));
    }
  }
}

void UBRGameInstance::RandomTeams() {
  UE_LOG(LogTemp, Log, TEXT("[GameInstance] RandomTeams 명령"));

  if (UWorld *World = GetWorld()) {
    if (APlayerController *PC = World->GetFirstPlayerController()) {
      if (ABRPlayerController *BRPC = Cast<ABRPlayerController>(PC)) {
        BRPC->RandomTeams();
      } else {
        UE_LOG(LogTemp, Error,
               TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
      }
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 "
                  "시작되지 않았을 수 있습니다."));
    }
  }
}

void UBRGameInstance::ChangeTeam(int32 PlayerIndex, int32 TeamNumber) {
  UE_LOG(LogTemp, Log,
         TEXT("[GameInstance] ChangeTeam 명령: PlayerIndex=%d, TeamNumber=%d"),
         PlayerIndex, TeamNumber);

  if (UWorld *World = GetWorld()) {
    if (APlayerController *PC = World->GetFirstPlayerController()) {
      if (ABRPlayerController *BRPC = Cast<ABRPlayerController>(PC)) {
        BRPC->ChangeTeam(PlayerIndex, TeamNumber);
      } else {
        UE_LOG(LogTemp, Error,
               TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
      }
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 "
                  "시작되지 않았을 수 있습니다."));
    }
  }
}

void UBRGameInstance::StartGame() {
  UE_LOG(LogTemp, Log, TEXT("[GameInstance] StartGame 명령"));

  if (UWorld *World = GetWorld()) {
    if (APlayerController *PC = World->GetFirstPlayerController()) {
      if (ABRPlayerController *BRPC = Cast<ABRPlayerController>(PC)) {
        BRPC->StartGame();
      } else {
        UE_LOG(LogTemp, Error,
               TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
      }
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 "
                  "시작되지 않았을 수 있습니다."));
    }
  }
}

void UBRGameInstance::ShowRoomInfo() {
  UE_LOG(LogTemp, Log, TEXT("[GameInstance] ShowRoomInfo 명령"));

  if (UWorld *World = GetWorld()) {
    if (APlayerController *PC = World->GetFirstPlayerController()) {
      if (ABRPlayerController *BRPC = Cast<ABRPlayerController>(PC)) {
        BRPC->ShowRoomInfo();
      } else {
        UE_LOG(LogTemp, Error,
               TEXT("[GameInstance] BRPlayerController를 찾을 수 없습니다."));
      }
    } else {
      UE_LOG(LogTemp, Error,
             TEXT("[GameInstance] PlayerController를 찾을 수 없습니다. 게임이 "
                  "시작되지 않았을 수 있습니다."));
    }
  }
}

// Seamless Travel 시 GameInstance가 달라질 수 있어, 프로세스 정적 저장소에 백업
// (복원 시 사용)
namespace {
TMap<FString, TTuple<int32, bool, int32>> G_PendingRoleByName;
TArray<TTuple<int32, bool, int32>> G_PendingRoleByIndex;
} // namespace

void UBRGameInstance::SavePendingRolesForTravel(ABRGameState *GameState) {
  if (!GameState)
    return;
  PendingRoleRestoreByName.Empty();
  PendingRoleRestoreByIndex.Empty();
  G_PendingRoleByName.Empty();
  G_PendingRoleByIndex.Empty();
  for (APlayerState *PS : GameState->PlayerArray) {
    if (ABRPlayerState *BRPS = Cast<ABRPlayerState>(PS)) {
      TTuple<int32, bool, int32> Data(BRPS->TeamNumber, BRPS->bIsLowerBody,
                                      BRPS->ConnectedPlayerIndex);
      const FString LocalPlayerName = BRPS->GetPlayerName();
      const FString LocalUserUID = BRPS->UserUID;
      // Travel 후 복원 시 이름 복제가 늦을 수 있으므로, 이름과 UID 둘 다 키로 저장
      if (!LocalPlayerName.IsEmpty()) {
        PendingRoleRestoreByName.Add(LocalPlayerName, Data);
        G_PendingRoleByName.Add(LocalPlayerName, Data);
      }
      if (!LocalUserUID.IsEmpty() && LocalUserUID != LocalPlayerName) {
        PendingRoleRestoreByName.Add(LocalUserUID, Data);
        G_PendingRoleByName.Add(LocalUserUID, Data);
      }
      G_PendingRoleByIndex.Add(Data);
      PendingRoleRestoreByIndex.Add(Data);
    }
  }
  UE_LOG(LogTemp, Warning,
         TEXT("[랜덤 팀 적용] Seamless Travel 전 역할 저장: %d명 (정적+GI)"),
         G_PendingRoleByIndex.Num());
}

void UBRGameInstance::RestorePendingRolesFromTravel(ABRGameState *GameState) {
  if (!GameState) {
    UE_LOG(LogTemp, Warning,
           TEXT("[랜덤 팀 적용] 역할 복원 스킵: GameState 없음"));
    return;
  }
  // GameInstance 데이터가 비어 있으면 정적 백업 사용 (멀티 PIE 등에서 GI가
  // 달라질 수 있음)
  bool bUseStatic = (PendingRoleRestoreByName.Num() == 0 &&
                     PendingRoleRestoreByIndex.Num() == 0);
  if (bUseStatic && G_PendingRoleByIndex.Num() == 0 &&
      G_PendingRoleByName.Num() == 0) {
    UE_LOG(
        LogTemp, Warning,
        TEXT(
            "[랜덤 팀 적용] 역할 복원 스킵: 저장된 역할 없음 (GI=%d, 정적=%d)"),
        PendingRoleRestoreByIndex.Num(), G_PendingRoleByIndex.Num());
    return;
  }
  int32 Restored = 0;
  auto &NameMap = bUseStatic ? G_PendingRoleByName : PendingRoleRestoreByName;
  auto &IndexArr =
      bUseStatic ? G_PendingRoleByIndex : PendingRoleRestoreByIndex;
  // 1) PlayerName으로 복원 시도
  for (APlayerState *PS : GameState->PlayerArray) {
    if (ABRPlayerState *BRPS = Cast<ABRPlayerState>(PS)) {
      FString Key = BRPS->GetPlayerName();
      if (Key.IsEmpty())
        Key = BRPS->UserUID;
      const TTuple<int32, bool, int32> *Found = NameMap.Find(Key);
      if (Found) {
        BRPS->SetTeamNumber(Found->Get<0>());
        BRPS->SetPlayerRole(Found->Get<1>(), Found->Get<2>());
        Restored++;
        UE_LOG(LogTemp, Log, TEXT("[진단] 복원 매칭(이름/UID) 성공: Key='%s' -> 팀%d %s"), *Key, Found->Get<0>(), Found->Get<1>() ? TEXT("하체") : TEXT("상체"));
      } else {
        UE_LOG(LogTemp, Warning, TEXT("[진단] 복원 매칭 실패: GetPlayerName='%s' UserUID='%s' (NameMap %d개)"), *BRPS->GetPlayerName(), *BRPS->UserUID, NameMap.Num());
      }
    }
  }
  // 2) 이름 매칭 실패 시 인덱스로 폴백
  if (Restored == 0 && IndexArr.Num() > 0) {
    const int32 N = FMath::Min(IndexArr.Num(), GameState->PlayerArray.Num());
    for (int32 i = 0; i < N; i++) {
      if (ABRPlayerState *BRPS =
              Cast<ABRPlayerState>(GameState->PlayerArray[i])) {
        const TTuple<int32, bool, int32> &Data = IndexArr[i];
        BRPS->SetTeamNumber(Data.Get<0>());
        BRPS->SetPlayerRole(Data.Get<1>(), Data.Get<2>());
        Restored++;
      }
    }
    UE_LOG(LogTemp, Warning,
           TEXT("[랜덤 팀 적용] Seamless Travel 후 역할 복원: %d명 (인덱스 "
                "폴백%s)"),
           Restored, bUseStatic ? TEXT(", 정적") : TEXT(""));
  } else {
    UE_LOG(
        LogTemp, Warning,
        TEXT("[랜덤 팀 적용] Seamless Travel 후 역할 복원: %d명 (이름 매칭%s)"),
        Restored, bUseStatic ? TEXT(", 정적") : TEXT(""));
  }
  // 저장 데이터는 즉시 비우지 않음. 재시도(하체 Pawn 대기) 시 다시 복원할 수 있도록 유지.
  // 실제 클리어는 GameMode에서 적용 성공/포기 시 ClearPendingRoleRestoreData() 호출.
}

void UBRGameInstance::ClearPendingRoleRestoreData() {
  PendingRoleRestoreByName.Empty();
  PendingRoleRestoreByIndex.Empty();
  G_PendingRoleByName.Empty();
  G_PendingRoleByIndex.Empty();
}

bool UBRGameInstance::HasPendingRoleRestore() const {
  return PendingRoleRestoreByName.Num() > 0 ||
         PendingRoleRestoreByIndex.Num() > 0 || G_PendingRoleByName.Num() > 0 ||
         G_PendingRoleByIndex.Num() > 0;
}

int32 UBRGameInstance::GetPendingRoleRestoreCount() const {
  if (PendingRoleRestoreByIndex.Num() > 0)
    return PendingRoleRestoreByIndex.Num();
  return G_PendingRoleByIndex.Num();
}

bool UBRGameInstance::HasPendingUserInfoForIndex(int32 Index) const {
  if (Index < 0)
    return false;
  if (PendingRoleRestoreByIndex.Num() > 0)
    return Index < PendingRoleRestoreByIndex.Num();
  return Index < G_PendingRoleByIndex.Num();
}

void UBRGameInstance::RestoreUserInfoToPlayerStateForPostLogin(
    ABRPlayerState *BRPS, int32 Index) {
  if (!BRPS || Index < 0)
    return;
  const TArray<TTuple<int32, bool, int32>> &Arr =
      (PendingRoleRestoreByIndex.Num() > 0) ? PendingRoleRestoreByIndex
                                           : G_PendingRoleByIndex;
  if (Index >= Arr.Num())
    return;
  const TTuple<int32, bool, int32> &Data = Arr[Index];
  BRPS->SetTeamNumber(Data.Get<0>());
  BRPS->SetPlayerRole(Data.Get<1>(), Data.Get<2>());
}

/** [핵심] JSON 데이터를 읽어 DT를 갱신하고 에셋으로 저장함 */
void UBRGameInstance::ReloadAllConfigs() {
  GI_LOG(Display, TEXT("=== Starting Global Config Reload and Asset Sync ==="));

  if (ConfigDataMap.Num() == 0) {
    GI_LOG(
        Warning,
        TEXT("ConfigDataMap이 비어 있습니다. 에디터에서 설정이 필요합니다."));
    return;
  }

  for (auto &Elem : ConfigDataMap) {
    const FString &JsonFileName = Elem.Key;
    UDataTable *TargetTable = Elem.Value;

    if (TargetTable) {
      // 1. JSON 파일 읽어서 메모리상 DT 업데이트
      UpdateDataTableFromJson(TargetTable, JsonFileName);

      // 2. 에디터 환경이고 게임이 실행 중이 아닌 경우에만 .uasset 파일로 영구
      // 저장 Standalone 모드나 PIE 모드에서는 저장하지 않음
#if WITH_EDITOR
      // 에디터에서만 저장하고, 게임 실행 중이 아닐 때만 저장
      // GetWorld()가 있으면 게임이 실행 중인 것으로 간주
      if (GIsEditor && !GetWorld()) {
        SaveDataTableToAsset(TargetTable);
      }
#endif
    }
  }

  // 글로벌 배율 적용 로직 호출
  ApplyGlobalMultipliers();

  // 3. 월드에 이미 존재하는 무기들에게 최신 데이터를 적용 (기존 로직 유지)
  if (GetWorld()) {
    for (TActorIterator<ABaseWeapon> It(GetWorld()); It; ++It) {
      It->LoadWeaponData();
    }
  }

  GI_LOG(Display, TEXT("=== Global Config Reload Complete ==="));
}

void UBRGameInstance::LoadConfigFromJson(const FString &FileName,
                                         UDataTable *TargetTable) {
  if (!TargetTable)
    return;

  // 경로: 프로젝트/Config/파일명.json
  FString FilePath = GetConfigDirectory() + FileName + TEXT(".json");
  FString JsonString;

  if (!FFileHelper::LoadFileToString(JsonString, *FilePath)) {
    GI_LOG(Warning, TEXT("File not found: %s"), *FilePath);
    return;
  }

  TSharedPtr<FJsonObject> RootObject;
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

  if (FJsonSerializer::Deserialize(Reader, RootObject) &&
      RootObject.IsValid()) {
    const TArray<TSharedPtr<FJsonValue>> *DataArray;
    // 모든 JSON의 최상위 배열 키를 "Data"로 통일하거나 파일명과 맞춥니다.
    if (RootObject->TryGetArrayField(TEXT("Data"), DataArray)) {
      // const UScriptStruct* 로 선언하여 타입 에러 해결
      const UScriptStruct *TableStruct = TargetTable->GetRowStruct();

      for (const auto &Value : *DataArray) {
        TSharedPtr<FJsonObject> DataObj = Value->AsObject();
        if (!DataObj.IsValid())
          continue;

        FName RowID = FName(*DataObj->GetStringField(TEXT("Name")));
        uint8 *RowPtr = TargetTable->FindRowUnchecked(RowID);

        if (RowPtr && TableStruct) {
          // 수치 데이터 주입
          FJsonObjectConverter::JsonObjectToUStruct(DataObj.ToSharedRef(),
                                                    TableStruct, RowPtr);
          GI_LOG(Log, TEXT("[%s.json] Row Updated: %s"), *FileName,
                 *RowID.ToString());
        }
      }
    }
  }
}

FString UBRGameInstance::GetConfigDirectory() {
  FString TargetPath;

#if WITH_EDITOR
  // 1. 에디터 환경: 프로젝트 루트의 Data 폴더
  TargetPath = FPaths::ProjectDir() / TEXT("Data/");
#else
  // 2. 패키징 환경: 빌드된 .exe 옆의 Data 폴더 (예:
  // Build/Windows/MyProject/Data/) FPaths::ProjectDir()는 패키징 후에도 실행
  // 파일 기준 경로를 반환합니다.
  TargetPath = FPaths::ProjectDir() / TEXT("Data/");
#endif

  return TargetPath;
}

/** JSON 문자열을 DataTable에 주입 */
void UBRGameInstance::UpdateDataTableFromJson(UDataTable *TargetTable,
                                              FString FileName) {
  if (!TargetTable)
    return;
    
  FString FullPath = GetConfigDirectory() + FileName + TEXT(".json");
  FString JsonString;

  if (!FFileHelper::LoadFileToString(JsonString, *FullPath)) {
    GI_LOG(Warning, TEXT("JSON 파일을 찾을 수 없습니다: %s"), *FullPath);
    return;
  }

  // 1. JSON 파싱
  TSharedPtr<FJsonObject> RootObject;
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

  if (FJsonSerializer::Deserialize(Reader, RootObject) &&
      RootObject.IsValid()) {
    const TArray<TSharedPtr<FJsonValue>> *DataArray;
    // JSON의 "Data" 배열 필드를 가져옴
    if (RootObject->TryGetArrayField(TEXT("Data"), DataArray)) {
      const UScriptStruct *TableStruct = TargetTable->GetRowStruct();

      for (const auto &Value : *DataArray) {
        TSharedPtr<FJsonObject> DataObj = Value->AsObject();
        if (!DataObj.IsValid())
          continue;

        // 1. 행 이름(Name) 확인
        FString NameStr = DataObj->GetStringField(TEXT("Name"));
        if (NameStr.IsEmpty())
          continue;

        FName RowName = FName(*NameStr);

        // 2. 기존 행 찾기
        uint8 *RowPtr = TargetTable->FindRowUnchecked(RowName);

        // 3. 행이 없으면 새로 추가
        if (!RowPtr)
        {
            if (TableStruct)
            {
                // 1. 메모리 할당 (구조체 크기만큼)
                uint8* NewRowData = (uint8*)FMemory::Malloc(TableStruct->GetStructureSize());

                // 2. 구조체 초기화 (생성자 호출 -> 여기서 포인터들이 nullptr로 안전하게 초기화됨)
                TableStruct->InitializeStruct(NewRowData);

                // 3. 테이블에 추가 (초기화된 데이터를 넣음)
                TargetTable->AddRow(RowName, *(FTableRowBase*)NewRowData);

                // 4. 임시 메모리 해제
                TableStruct->DestroyStruct(NewRowData);
                FMemory::Free(NewRowData);
            }

            // 포인터 다시 갱신 (이제 안전하게 생성된 행을 가리킴)
            RowPtr = TargetTable->FindRowUnchecked(RowName);
        }

        // 4. 데이터 주입 (기본적으로 기존 데이터는 유지하고 JSON에 있는 필드만
        // 덮어씀)
        if (RowPtr && TableStruct) {
          FJsonObjectConverter::JsonObjectToUStruct(DataObj.ToSharedRef(),
                                                    TableStruct, RowPtr);
          GI_LOG(Log, TEXT("[%s] 데이터 업데이트 완료: %s"), *FileName,
                 *RowName.ToString());
        }
      }

      // 데이터 테이블 구조 갱신 알림 (에디터 UI 등에 즉시 반영)
      TargetTable->Modify();

#if WITH_EDITOR
      // 에디터에게 데이터 테이블의 구조나 내용이 바뀌었음을 알림
      // 이 함수는 UDataTable에 정의되어 있으며, 에디터 UI를 즉시
      // 새로고침합니다.
      TargetTable->OnDataTableChanged().Broadcast();

      // (선택 사항) 데이터 테이블 에셋 아이콘에 별표(*) 표시 (수정됨 표시)
      TargetTable->PostEditChange();
#endif
    }
  }
}

/** 에셋 파일(.uasset)로 영구 저장 */
void UBRGameInstance::SaveDataTableToAsset(UDataTable *TargetTable) {
#if WITH_EDITOR
  // 게임 실행 중이면 저장하지 않음 (Standalone, PIE 모드 등)
  // GetWorld()가 있으면 게임이 실행 중인 것으로 간주
  if (GetWorld()) {
    GI_LOG(Warning, TEXT("게임 실행 중이므로 Asset 저장을 건너뜁니다."));
    return;
  }

  if (!TargetTable) {
    GI_LOG(Error, TEXT("TargetTable이 유효하지 않습니다."));
    return;
  }

  UPackage *Package = TargetTable->GetOutermost();
  if (!Package) {
    GI_LOG(Error, TEXT("Package를 찾을 수 없습니다."));
    return;
  }

  FString PackageFileName = FPackageName::LongPackageNameToFilename(
      Package->GetName(), FPackageName::GetAssetPackageExtension());

  FSavePackageArgs SaveArgs;
  SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
  SaveArgs.Error = GError;
  SaveArgs.bForceByteSwapping = true;

  if (UPackage::SavePackage(Package, TargetTable, *PackageFileName, SaveArgs)) {
    GI_LOG(Log, TEXT("Asset 영구 저장 성공: %s"), *PackageFileName);
  } else {
    GI_LOG(Error, TEXT("Asset 저장 실패: %s"), *PackageFileName);
  }
#else
  // 에디터가 아닌 환경에서는 저장하지 않음
  GI_LOG(Warning, TEXT("에디터가 아니므로 Asset 저장을 건너뜁니다."));
#endif
}

void UBRGameInstance::ApplyGlobalMultipliers() {
  if (UDataTable **TargetTablePtr =
          ConfigDataMap.Find(TEXT("GlobalSettings"))) {
    UDataTable *GlobalTable = *TargetTablePtr;
    if (GlobalTable) {
      static const FString ContextString(TEXT("Global Settings Context"));
      FGlobalBalanceData *FoundData = GlobalTable->FindRow<FGlobalBalanceData>(
          FName("Default"), ContextString);

      if (FoundData) {
        // 1. 무기 관련 전역 변수 업데이트
        ABaseWeapon::GlobalDamageMultiplier =
            FoundData->Global_Weapon_DamageMultiplier;
        ABaseWeapon::GlobalImpulseMultiplier =
            FoundData->Global_Weapon_ImpulseMultiplier;
        ABaseWeapon::GlobalAttackSpeedMultiplier =
            FoundData->Global_Weapon_AttackSpeedMultiplier;
        ABaseWeapon::GlobalDurabilityReduction =
            FoundData->Global_Durability_Reduction;

        // 2. 스태미나 관련 전역(static) 변수 업데이트
        UStaminaComponent::Global_SprintDrainRate =
            FoundData->Global_Stamina_SprintDrainRate;
        UStaminaComponent::Global_JumpCost = FoundData->Global_Stamina_JumpCost;
        UStaminaComponent::Global_RegenRate =
            FoundData->Global_Stamina_RegenRate;

        // 3. (핵심) 이미 소환된 캐릭터들에게도 즉시 적용 (실시간 리로드를 위해)
        if (UWorld *World = GetWorld()) {
          // 캐릭터 및 스태미나 업데이트
          for (TActorIterator<APlayerCharacter> It(World); It; ++It) {
            APlayerCharacter *PC = *It;

            // A. 스태미나 컴포넌트 업데이트 (기존 코드)
            if (UStaminaComponent *StaminaComp = PC->StaminaComp) {
              StaminaComp->StaminaDrainRate =
                  UStaminaComponent::Global_SprintDrainRate;
              StaminaComp->JumpCost = UStaminaComponent::Global_JumpCost;
              StaminaComp->StaminaRegenRate =
                  UStaminaComponent::Global_RegenRate;
            }

            // B. 캐릭터가 장착 중인 무기 업데이트
            // (캐릭터에 GetCurrentWeapon() 같은 접근자가 있다고 가정)
            if (ABaseWeapon *CharacterWeapon = PC->CurrentWeapon) {
              // 내구도 감소량 등 인스턴스 변수 갱신
              CharacterWeapon->DurabilityReduction =
                  ABaseWeapon::GlobalDurabilityReduction;
            }
          }

          // C. 바닥에 떨어져 있는(장착되지 않은) 무기들도 업데이트
          for (TActorIterator<ABaseWeapon> It(World); It; ++It) {
            ABaseWeapon *Weapon = *It;
            Weapon->DurabilityReduction =
                ABaseWeapon::GlobalDurabilityReduction;
          }
        }

        GI_LOG(Display,
               TEXT("스태미나 세팅 적용. Stamina: Drain(%.1f), Jump(%.1f), "
                    "Regen(%.1f)"),
               UStaminaComponent::Global_SprintDrainRate,
               UStaminaComponent::Global_JumpCost,
               UStaminaComponent::Global_RegenRate);

        GI_LOG(Display,
               TEXT("무기 배율 세팅 적용. Weapon: Damage(%.1f), Impulse(%.1f), "
                    "AttackSpeed(%.1f)"),
               ABaseWeapon::GlobalDamageMultiplier,
               ABaseWeapon::GlobalImpulseMultiplier,
               ABaseWeapon::GlobalAttackSpeedMultiplier);
      }
    }
  }
}

void UBRGameInstance::DoPIEExitCleanup(UWorld *World) {
  if (!World || !World->IsPlayInEditor()) {
    return;
  }
  GI_LOG(Warning,
         TEXT("PIE 종료 정리(DoPIEExitCleanup) - World 참조 사슬 해제"));

  // 1) SessionInterface→GameSession→World 참조를 가장 먼저 끊음 (UnrealEdEngine
  // 경로의 참조 원인 제거)
  if (AGameModeBase *GameMode = World->GetAuthGameMode()) {
    if (ABRGameSession *GameSession =
            Cast<ABRGameSession>(GameMode->GameSession)) {
      GameSession->UnbindSessionDelegatesForPIEExit();
    }
    OnRoomTitleReceived.Clear();
  }

  // 2) GEngine/GameSession 델리게이트·위젯 정리 — PC가 월드를 잡지 않도록
  if (APlayerController *PC = World->GetFirstPlayerController()) {
    if (ABRPlayerController *BRPC = Cast<ABRPlayerController>(PC)) {
      BRPC->ClearUIForShutdown();
    }
  }

  // 3) 타이머 정리 — 콜백이 월드/세션을 잡고 있지 않도록
  World->GetTimerManager().ClearTimer(ListenServerTimerHandle);
  World->GetTimerManager().ClearTimer(SessionRecreateTimerHandle);
  World->GetTimerManager().ClearAllTimersForObject(this);

  // 4) 델리게이트 정리 — 위젯이 GameInstance 델리게이트에 바인딩된 경우 월드
  // 참조가 남음
  OnRoomTitleReceived.Clear();

  // 4) NavigationSystem 정리 (월드 파괴 직전 호출 시 크래시 가능성 있음 —
  // 마지막에 수행)
  if (UNavigationSystemV1 *NavSys =
          FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)) {
    NavSys->CleanUp();
  }
}

void UBRGameInstance::Shutdown() {
  // 전역 델리게이트 등록 해제 — 해제되지 않으면 엔진이 우리 콜백을 들고 있어
  // 월드 참조 사슬이 남을 수 있음
  if (OnWorldCleanupHandle.IsValid()) {
    FWorldDelegates::OnWorldCleanup.Remove(OnWorldCleanupHandle);
    OnWorldCleanupHandle.Reset();
  }

#if WITH_EDITOR
  if (PrePIEEndedHandle.IsValid()) {
	  FEditorDelegates::PrePIEEnded.Remove(PrePIEEndedHandle);
	  PrePIEEndedHandle.Reset();
  }
#endif

  // PIE 종료 시 모든 PIE 월드에 대해 정리 (GetWorld()만 쓰면 맵 이동 후
  // null/다른 월드일 수 있음)
  if (GEngine) {
    const auto &Contexts = GEngine->GetWorldContexts();
    for (const FWorldContext &Context : Contexts) {
      UWorld *World = Context.World();
      if (World && World->IsPlayInEditor() &&
          Context.OwningGameInstance == this) {
        DoPIEExitCleanup(World);
      }
    }
  }
  // 위에서 한 번 정리했어도, 현재 월드가 아직 올라와 있을 수 있으므로 한 번 더
  UWorld *CurrentWorld = GetWorld();
  if (CurrentWorld && CurrentWorld->IsPlayInEditor()) {
    DoPIEExitCleanup(CurrentWorld);
  }

  Super::Shutdown();
}

void UBRGameInstance::LoadPlayerNameFromUserInfo() {
  // S_UserInfo 에셋에서 PlayerName 로드 시도
  // 에셋 경로: /Game/Main/Data/S_UserInfo
  const FString AssetPath = TEXT("/Game/Main/Data/S_UserInfo.S_UserInfo");

  UObject *LoadedAsset =
      StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
  if (LoadedAsset) {
    UE_LOG(LogTemp, Log,
           TEXT("[GameInstance] S_UserInfo 에셋 로드됨. 클래스: %s"),
           *LoadedAsset->GetClass()->GetName());

    // 에셋의 모든 속성 나열 (디버그용)
    for (TFieldIterator<FProperty> PropIt(LoadedAsset->GetClass()); PropIt;
         ++PropIt) {
      FProperty *Property = *PropIt;
      UE_LOG(LogTemp, Log, TEXT("[GameInstance] 속성 발견: %s (타입: %s)"),
             *Property->GetName(), *Property->GetCPPType());
    }

    // PlayerName 속성 찾기 (다양한 이름 시도)
    TArray<FName> PossibleNames = {FName("PlayerName"), FName("Name"),
                                   FName("UserName"), FName("DisplayName")};

    for (const FName &PropName : PossibleNames) {
      FProperty *NameProperty =
          LoadedAsset->GetClass()->FindPropertyByName(PropName);
      if (NameProperty) {
        FString LoadedName;
        if (FStrProperty *StrProp = CastField<FStrProperty>(NameProperty)) {
          LoadedName = StrProp->GetPropertyValue_InContainer(LoadedAsset);
        } else if (FTextProperty *TextProp =
                       CastField<FTextProperty>(NameProperty)) {
          LoadedName =
              TextProp->GetPropertyValue_InContainer(LoadedAsset).ToString();
        } else if (FNameProperty *FNameProp =
                       CastField<FNameProperty>(NameProperty)) {
          LoadedName =
              FNameProp->GetPropertyValue_InContainer(LoadedAsset).ToString();
        }

        if (!LoadedName.IsEmpty()) {
          PlayerName = LoadedName;
          UE_LOG(LogTemp, Log,
                 TEXT("[GameInstance] S_UserInfo에서 %s 로드 성공: %s"),
                 *PropName.ToString(), *PlayerName);
          return;
        }
      }
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[GameInstance] S_UserInfo 에셋에서 이름 속성을 찾을 수 "
                "없습니다."));
  } else {
    UE_LOG(
        LogTemp, Warning,
        TEXT("[GameInstance] S_UserInfo 에셋을 로드할 수 없습니다. 경로: %s"),
        *AssetPath);
  }

  // 에셋 로드 실패 시 "Player" 사용 (빈 값이면 UID가 표시되는 문제 방지.
  // "Player_1234" 대신 깔끔한 기본값)
  if (PlayerName.IsEmpty()) {
    PlayerName = TEXT("Player");
    UE_LOG(LogTemp, Warning,
           TEXT("[GameInstance] S_UserInfo에서 이름 로드 실패. 기본값 'Player' "
                "사용"));
  }
}

void UBRGameInstance::SaveCustomization(const FBRCustomizationData &NewData)
{
    LocalCustomizationData = NewData;
    LocalCustomizationData.bIsDataValid = true;

    UE_LOG(LogBRGameInstance, Log,
            TEXT("Local Customization Saved: Head(%d), Leg(%d)"), NewData.HeadID,
            NewData.LegID);
}