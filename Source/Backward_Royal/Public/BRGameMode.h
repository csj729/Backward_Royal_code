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
	TArray<FString> StageMapPathsFallback = { 
		//TEXT("/Game/Main/Level/Stage/Stage01_Temple"),
		//TEXT("/Game/Main/Level/Stage/Stage02_Bushes"),
		TEXT("/Game/Main/Level/Stage/Stage03_Arena"),
		//TEXT("/Game/Main/Level/Stage/Stage04_Race") 
		};

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

	// 팀별 상체 스폰 간격(초). 이전 팀 상체 스폰/빙의/복제가 완료된 뒤에만 다음 팀 진행 (1팀 하체→상체 완료 후 2팀 진행)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float SpawnDelayBetweenTeams = 1.2f;

	/** 팀에 배정된 인원(TeamNumber>0, 대기열 관전 제외)만으로 고정 스폰 수: N명 시 하체 N/2, 상체 N/2. 4명→하체2 상체2, 6명→하체3 상체3 */
	static int32 GetExpectedLowerBodyCount(int32 NumPlayersInTeamsOnly) { return NumPlayersInTeamsOnly / 2; }
	static int32 GetExpectedUpperBodyCount(int32 NumPlayersInTeamsOnly) { return NumPlayersInTeamsOnly / 2; }

	// 플레이어 사망 시 호출되는 함수
	void OnPlayerDied(class ABaseCharacter* VictimCharacter);

	// 2초 후 팀 전체를 관전 모드로 전환하는 함수
	void SwitchTeamToSpectator(TWeakObjectPtr<class ABRPlayerController> VictimPC, TWeakObjectPtr<class ABRPlayerController> PartnerPC);

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
	/** 순차 상체 스폰에서 실제로 스폰된 상체 수 (완료 시 로그/경고용) */
	int32 StagedUpperBodiesSpawnedCount = 0;
	/** 전체 하체 Pawn 준비 대기 재시도 횟수 (로딩 빠른 맵에서 전원 하체 스폰 완료 후 상체 적용) */
	int32 StagedAllLowerReadyRetries = 0;
	FTimerHandle StagedAllLowerReadyHandle;
	/** 하체 Pawn 대기 재진입 시 팀 목록 고정용. 재진입 시 PlayerArray 기준 재구성하면 일부가 TeamNumber 0으로 바뀌어 3명만 남는 현상 방지 */
	TArray<ABRPlayerState*> PendingSortedByTeamSnapshot;
	/** 팀별 상체 스폰 시 하체 Pawn 없을 때 같은 팀 재시도 횟수 (Stage02_Bushes 등 느린 맵 대응) */
	int32 StagedPawnWaitRetriesForTeam = 0;
	static constexpr int32 MaxStagedPawnWaitRetriesPerTeam = 8;
	/** 팀별 Controller 없을 때 같은 팀 재시도. 1팀 하체→상체 완료 후 다음 팀 진행하듯, 해당 팀 Controller가 서버에 올 때까지 대기 */
	int32 StagedControllerWaitRetriesForTeam = 0;
	static constexpr int32 MaxStagedControllerWaitRetriesForTeam = 40;  // 0.5초×40 = 20초 (하체 Pawn 대기와 동일하게 생성될 때까지 대기)
	FTimerHandle StagedApplyTimerHandle;
	FTimerHandle InitialRoleApplyTimerHandle;
	/** 테스트 맵 직접 실행(로비 없음) 시 2초 후 역할 적용 폴백용 */
	FTimerHandle DirectStartRoleApplyTimerHandle;

	/** 테스트 맵을 로비 없이 바로 실행했을 때: 저장된 역할이 없고 전원 하체면 랜덤 팀 배정 후 상체/하체 적용 */
	void TryApplyDirectStartRolesFallback();

	/** 플레이어 대기 재시도 횟수. 최대 초과 시 현재 인원으로 진행해 상체 스폰 시도 (하체만 4명 방지) */
	int32 InitialPlayerWaitRetries = 0;
	static constexpr int32 MaxInitialPlayerWaitRetries = 24;  // 0.5초×24 = 12초

	/** 1.5초 폴백 타이머가 이미 예약되었으면 true (OnPossess 중복 예약 방지) */
	bool bHasScheduledInitialRoleApply = false;
public:
	bool HasScheduledInitialRoleApply() const { return bHasScheduledInitialRoleApply; }
	/** 1.5초 후 ApplyRoleChangesForRandomTeams 예약 (BeginPlay/OnPossess에서 한 번만 호출되도록 외부에서 HasScheduledInitialRoleApply 확인 후 호출) */
	void ScheduleInitialRoleApplyIfNeeded();
};

