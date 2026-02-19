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

	/** 게임 진행 중(로비가 아닌 스테이지 맵)일 때 새 플레이어 입장 차단. true면 차단, false면 항상 허용(기본: true) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings", meta = (DisplayName = "게임 진행 중 입장 차단"))
	bool bBlockJoinWhenGameStarted = true;

	/** 연결 시도 시 호출. ErrorMessage를 설정하면 해당 플레이어 입장 거부 */
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

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

	/** 생존 팀이 1개일 때 승리 처리. SwitchTeamToSpectatorByPlayerIndices에서 호출 */
	void CheckAndEndGameIfWinner();

	// 2초 후 팀 전체를 관전 모드로 전환 (WeakPtr 버전, 레거시)
	void SwitchTeamToSpectator(TWeakObjectPtr<class ABRPlayerController> VictimPC, TWeakObjectPtr<class ABRPlayerController> PartnerPC);

	/** 사망 팀 관전 전환 — PlayerArray 인덱스로 호출 (타이머 콜백용, 인덱스로 컨트롤러 재조회) */
	void SwitchTeamToSpectatorByPlayerIndices(int32 VictimPlayerIndex, int32 PartnerPlayerIndex);

	/** 탈락한 팀 번호 기준으로 해당 팀 전원(하체+상체) 관전 전환 */
	void SwitchEliminatedTeamToSpectator(int32 EliminatedTeamNumber);

	/** 로비 맵으로 모든 플레이어 이동 (승리 시 WBP_LobbyMenu/WBP_Entry 표시) */
	void TravelToLobby();

	/** 승리 후 로비 이동 전 대기 시간(초). 0이면 즉시 이동 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float DelayBeforeReturnToLobby = 2.0f;

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
	/** 사망 후 2초 뒤 관전 전환용 (인덱스 콜백 사용) */
	FTimerHandle SpecTimerHandle_DeathSpectator;
	/** 승리 후 로비 이동용 타이머 */
	FTimerHandle ReturnToLobbyTimerHandle;

	/** 테스트 맵을 로비 없이 바로 실행했을 때: 저장된 역할이 없고 전원 하체면 랜덤 팀 배정 후 상체/하체 적용 */
	void TryApplyDirectStartRolesFallback();

	/** 플레이어 대기 재시도 횟수. 최대 초과 시 현재 인원으로 진행해 상체 스폰 시도 (하체만 4명 방지) */
	int32 InitialPlayerWaitRetries = 0;
	static constexpr int32 MaxInitialPlayerWaitRetries = 24;  // 0.5초×24 = 12초
	/** 2명 미만일 때 재시도 횟수. Seamless Travel 직후 클라이언트 재접속 전에 타이머가 돌면 하체만 스폰되므로, 2명 될 때까지 0.5초 간격 재시도 */
	int32 MinPlayerWaitRetries = 0;
	static constexpr int32 MaxMinPlayerWaitRetries = 24;  // 0.5초×24 = 12초
	/** ExpectedCount==0일 때 2명에서 바로 진행하면 나중에 들어온 3·4번째가 전부 하체로 남음. 2명일 때만 추가로 잠시 대기(0.5초×6=3초) */
	int32 ExpectedZeroWaitRetries = 0;
	static constexpr int32 MaxExpectedZeroWaitRetries = 6;

	/** 1.5초 폴백 타이머가 이미 예약되었으면 true (OnPossess 중복 예약 방지) */
	bool bHasScheduledInitialRoleApply = false;
public:
	bool HasScheduledInitialRoleApply() const { return bHasScheduledInitialRoleApply; }
	/** 1.5초 후 ApplyRoleChangesForRandomTeams 예약 (BeginPlay/OnPossess에서 한 번만 호출되도록 외부에서 HasScheduledInitialRoleApply 확인 후 호출) */
	void ScheduleInitialRoleApplyIfNeeded();
};

