// BRGameInstance.cpp
#include "BRGameInstance.h"
#include "BRPlayerController.h"
#include "BRGameSession.h"
#include "BRGameMode.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "BaseWeapon.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/GameModeBase.h"

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
	ReloadAllConfigs();
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

			// 2. 에디터 환경인 경우 .uasset 파일로 영구 저장
		#if WITH_EDITOR
			SaveDataTableToAsset(TargetTable);
		#endif
		}
	}

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
	if (!TargetTable) return;

	UPackage* Package = TargetTable->GetOutermost();
	if (!Package) return;

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
#endif
}