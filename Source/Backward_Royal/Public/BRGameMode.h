// BRGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "BRGameMode.generated.h"

class ABRPlayerState;

UCLASS()
class BACKWARD_ROYAL_API ABRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABRGameMode();

	// 최소 플레이어 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Room Settings")
	int32 MinPlayers = 4;

	// 최대 플레이어 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Room Settings")
	int32 MaxPlayers = 8;

	// 방 생성 후 이동할 로비 맵 경로. 비어 있으면 현재 맵 유지.
	// 예: /Game/Main/Level/Main_Scene 또는 /Game/Main/Level/Stage/Stage01_Temple
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings", meta = (DisplayName = "로비 맵 경로"))
	FString LobbyMapPath;

	// 게임 시작 맵 경로 (레거시 - 랜덤 맵 선택 시 폴백용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings")
	FString GameMapPath = TEXT("/Game/Main/Level/Stage/Stage01_Temple");

	/** Stage 폴더 경로. 이 경로 안의 모든 월드(맵) 에셋 중에서 랜덤 선택됩니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings", meta = (DisplayName = "Stage 폴더 경로"))
	FString StageFolderPath = TEXT("/Game/Main/Level/Stage");

	/** Stage 맵 목록 (폴백용). Stage 폴더 자동 수집이 실패할 때만 사용됩니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings")
	TArray<FString> StageMapPathsFallback;

	// 랜덤 맵 선택 사용 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings")
	bool bUseRandomMap = true;

	/** PIE 종료 시 GameMode→GameSession→World 참조 사슬 차단 (DoPIEExitCleanup에서 호출) */
	void ClearGameSessionForPIEExit();

	// 게임 시작
	UFUNCTION(BlueprintCallable, Category = "Game")
	void StartGame();

	// 플레이어 로그인 처리
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 플레이어 로그아웃 처리
	virtual void Logout(AController* Exiting) override;

	// 랜덤 팀 배정 후 상체/하체 Pawn 재배치 (상체 스폰 및 빙의)
	void ApplyRoleChangesForRandomTeams();

	// 팀별 상체 스폰 간격(초). 순차 스폰으로 복제/초기화 타이밍 버그 완화
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnDelayBetweenTeams = 0.3f;

	// 플레이어 사망 시 호출되는 함수
	void OnPlayerDied(class ABaseCharacter* VictimCharacter);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Stage 폴더에서 맵 목록을 수집하거나, 실패 시 StageMapPathsFallback 반환 */
	TArray<FString> GetAvailableStageMapPaths() const;

	// 에디터(BP_BRGameMode)에서 BP_UpperBodyPawn을 할당할 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Classes")
	TSubclassOf<class AUpperBodyPawn> UpperBodyClass;

	// 순차 스폰용: 한 팀씩 상체 스폰 후 다음 팀 예약
	void ApplyRoleChangesForRandomTeams_ApplyOneTeam();

	// 순차 스폰 스테이징 (팀 인덱스만 저장, SortedByTeam는 매번 GameState에서 재구성하지 않고 캐시)
	TArray<ABRPlayerState*> StagedSortedByTeam;
	int32 StagedNumTeams = 0;
	int32 StagedCurrentTeamIndex = 0;
	/** 팀별 상체 스폰 시 하체 Pawn 없을 때 같은 팀 재시도 횟수 (Stage02_Bushes 등 느린 맵 대응) */
	int32 StagedPawnWaitRetriesForTeam = 0;
	static constexpr int32 MaxStagedPawnWaitRetriesPerTeam = 8;
	FTimerHandle StagedApplyTimerHandle;
	FTimerHandle InitialRoleApplyTimerHandle;

	/** 1.5초 폴백 타이머가 이미 예약되었으면 true (OnPossess 중복 예약 방지) */
	bool bHasScheduledInitialRoleApply = false;
public:
	bool HasScheduledInitialRoleApply() const { return bHasScheduledInitialRoleApply; }
	/** 1.5초 후 ApplyRoleChangesForRandomTeams 예약 (BeginPlay/OnPossess에서 한 번만 호출되도록 외부에서 HasScheduledInitialRoleApply 확인 후 호출) */
	void ScheduleInitialRoleApplyIfNeeded();
};

