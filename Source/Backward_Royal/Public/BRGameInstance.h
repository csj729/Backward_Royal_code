// BRGameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WeaponTypes.h"
#include "ArmorTypes.h"
#include "BRGameInstance.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBRGameInstance, Log, All);

UCLASS()
class BACKWARD_ROYAL_API UBRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UBRGameInstance();
	
	virtual void Init() override;

	// 방 생성
	UFUNCTION(Exec)
	void CreateRoom(const FString& RoomName = TEXT("TestRoom"));

	// 방 찾기
	UFUNCTION(Exec)
	void FindRooms();

	// 준비 상태 토글
	UFUNCTION(Exec)
	void ToggleReady();

	// 랜덤 팀 배정
	UFUNCTION(Exec)
	void RandomTeams();

	// 플레이어 팀 변경
	UFUNCTION(Exec)
	void ChangeTeam(int32 PlayerIndex, int32 TeamNumber);

	// 게임 시작
	UFUNCTION(Exec)
	void StartGame();

	// 현재 상태 확인
	UFUNCTION(Exec)
	void ShowRoomInfo();

	/** * [확장형 구조]
	 * Key: JSON 파일 이름 (확장자 제외, 예: "WeaponBalance")
	 * Value: 매칭될 데이터 테이블 에셋
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data|Config")
	TMap<FString, class UDataTable*> ConfigDataMap;

	// --- [핵심] JSON 로드 및 밸런싱 적용 ---
	UFUNCTION(Exec, Category = "Data")
	void ReloadAllConfigs();

	/** JSON 파일을 읽어 데이터 테이블 업데이트 */
	UFUNCTION(BlueprintCallable, Category = "Data")
	void UpdateDataTableFromJson(UDataTable* TargetTable, FString FileName);

	/** 데이터 테이블 변경 사항을 .uasset 파일로 영구 저장 (에디터 전용) */
	void SaveDataTableToAsset(UDataTable* TargetTable);

	// 플레이어 이름 저장 및 가져오기
	UPROPERTY(BlueprintReadWrite, Category = "Player")
	FString PlayerName;

	UFUNCTION(BlueprintCallable, Category = "Player")
	FString GetPlayerName() const { return PlayerName; }

	UFUNCTION(BlueprintCallable, Category = "Player")
	void SetPlayerName(const FString& NewPlayerName) { PlayerName = NewPlayerName; }
	// 전역 변수 설정을 위한 함수
	void ApplyGlobalMultipliers();
		
protected:
	// 실제 JSON 파싱 로직
	void LoadConfigFromJson(const FString& FileName, class UDataTable* TargetTable);

	FString GetConfigDirectory();
};

