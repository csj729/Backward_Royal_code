// BRGameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "WeaponTypes.h"
#include "ArmorTypes.h"
#include "CustomizationInfo.h"
#include "BRGameInstance.generated.h"

class ABRPlayerState;

/** Seamless Travel 시 PlayerState에 복원할 전체 UserInfo (저장/복원용) */
struct FTravelUserInfoSave
{
	FString UserUID;
	FString PlayerName;
	FBRCustomizationData CustomizationData;
	bool bIsHost = false;
	bool bIsReady = false;
	int32 TeamNumber = 0;
	bool bIsSpectatorSlot = false;
	bool bIsLowerBody = true;
	int32 ConnectedPlayerIndex = -1;
};

DECLARE_LOG_CATEGORY_EXTERN(LogBRGameInstance, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomTitleReceived);

UCLASS()
class BACKWARD_ROYAL_API UBRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UBRGameInstance();
	
	virtual void Init() override;
	virtual void OnStart() override;
	virtual void Shutdown() override;

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

	/** 세션 동안 유지되는 UserUID (방 입장/게임 시작 시 초기화 방지) */
	UPROPERTY(BlueprintReadWrite, Category = "Player")
	FString UserUID;

	UFUNCTION(BlueprintCallable, Category = "Player")
	FString GetUserUID() const { return UserUID; }

	UFUNCTION(BlueprintCallable, Category = "Player")
	void SetUserUID(const FString& NewUserUID) { UserUID = NewUserUID; }

	/** S_UserInfo 에셋에서 PlayerName 로드 */
	void LoadPlayerNameFromUserInfo();

	// LAN 전용(true) / 인터넷(Steam) 매칭(false). 방 생성·방 찾기 시 사용.
	// 기본값: false (인터넷 매칭) - Steam을 통한 인터넷 매칭 사용
	// 콘솔 명령어: SetLANOnly 1 (LAN 전용) / SetLANOnly 0 (인터넷 매칭)
	UPROPERTY(BlueprintReadWrite, Category = "Session|Match")
	bool bUseLANOnly = false;

	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	void SetUseLANOnly(bool bLAN) { bUseLANOnly = bLAN; }

	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	bool GetUseLANOnly() const { return bUseLANOnly; }

	/** 콘솔: SetLANOnly 1 (LAN 전용) / SetLANOnly 0 (인터넷) */
	UFUNCTION(Exec, Category = "Session|Match")
	void SetLANOnly(int32 bEnabled);

	/** 방 생성 성공 후 ServerTravel 직전에 설정. 맵 재로드 후 로비 UI 표시 판단용. */
	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	void SetDidCreateRoomThenTravel(bool b) { bDidCreateRoomThenTravel = b; }

	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	bool GetDidCreateRoomThenTravel() const { return bDidCreateRoomThenTravel; }

	/** 방 생성 시 방 이름 저장 (맵 재로드 후 세션 재생성용) */
	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	void SetPendingRoomName(const FString& RoomName) { PendingRoomName = RoomName; }

	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	FString GetPendingRoomName() const { return PendingRoomName; }

	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	void ClearPendingRoomName() { PendingRoomName.Empty(); }

	/** 클라이언트 입장 시 RPC로 받은 방 제목 캐시. "○○'s Game" 즉시 표시용 */
	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	void SetCachedRoomTitle(const FString& Title) { CachedRoomTitle = Title; }

	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	FString GetCachedRoomTitle() const { return CachedRoomTitle; }

	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	void ClearCachedRoomTitle() { CachedRoomTitle.Empty(); }

	/** RPC로 방 제목 수신 시 브로드캐스트 (로비 UI에서 바인딩해 제목 즉시 갱신) */
	UPROPERTY(BlueprintAssignable, Category = "Session|Match")
	FOnRoomTitleReceived OnRoomTitleReceived;

	/** 로비에서 랜덤 팀 배정 후, 게임 맵 로드 시 상체/하체 Pawn 적용 대기 플래그 */
	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	void SetPendingApplyRandomTeamRoles(bool b) { bPendingApplyRandomTeamRoles = b; }

	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	bool GetPendingApplyRandomTeamRoles() const { return bPendingApplyRandomTeamRoles; }

	UFUNCTION(BlueprintCallable, Category = "Session|Match")
	void ClearPendingApplyRandomTeamRoles() { bPendingApplyRandomTeamRoles = false; }

	/** Seamless Travel 전에 호출: 현재 GameState의 팀/역할을 저장 (Travel 후 PlayerState가 초기화되므로 복원용) */
	void SavePendingRolesForTravel(class ABRGameState* GameState);

	/** 게임 맵 로드 후 ApplyRoleChangesForRandomTeams 내부에서 호출: 저장된 팀/역할을 PlayerState에 복원 */
	void RestorePendingRolesFromTravel(class ABRGameState* GameState);

	/** 역할 복원용 저장 데이터 비우기 (적용 성공/포기 시 GameMode에서만 호출) */
	void ClearPendingRoleRestoreData();

	/** Travel 복원 대기 중인지 (게임 맵 PostLogin 직후 UpdatePlayerList에서 새 플레이어→대기열 초기화 스킵용) */
	bool HasPendingRoleRestore() const;
	/** 저장된 역할 복원 데이터 개수 (ApplyRoleChangesForRandomTeams 대기 조건용) */
	int32 GetPendingRoleRestoreCount() const;

	/** PostLogin에서 호출: Travel 복원 시 해당 인덱스의 UserInfo가 있으면 true */
	bool HasPendingUserInfoForIndex(int32 Index) const;
	/** PostLogin에서 호출: 저장된 UserInfo를 PlayerState에 즉시 복원 (UID, Name, Customization, Host, Ready) */
	void RestoreUserInfoToPlayerStateForPostLogin(class ABRPlayerState* BRPS, int32 Index);

	// 전역 변수 설정을 위한 함수
	void ApplyGlobalMultipliers();

	// 로컬 플레이어가 선택한 커스터마이징 정보 (UI에서 이 값을 수정)
	UPROPERTY(BlueprintReadWrite, Category = "Customization")
	FBRCustomizationData LocalCustomizationData;

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SaveCustomization(const FBRCustomizationData& NewData);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	FBRCustomizationData GetLocalCustomization() const { return LocalCustomizationData; }
		
protected:
	// 실제 JSON 파싱 로직
	void LoadConfigFromJson(const FString& FileName, class UDataTable* TargetTable);

	FString GetConfigDirectory();

	/** Session/타이머/네비 등 정리 (Shutdown PIE 블록과 OnWorldCleanup 콜백에서 호출) */
	void DoPIEExitCleanup(UWorld* World);

	/** 방 생성 후 ServerTravel 호출 직전에 true 설정. BeginPlay에서 로비 표시 여부 판단에 사용. */
	bool bDidCreateRoomThenTravel = false;

	/** 방 생성 시 방 이름 저장 (맵 재로드 후 세션 재생성용) */
	FString PendingRoomName;

	/** 클라이언트 입장 시 RPC로 받은 방 제목 캐시 */
	FString CachedRoomTitle;

	/** 로비에서 랜덤 팀 배정 후, 게임 맵에서 ApplyRoleChangesForRandomTeams 호출 대기 */
	bool bPendingApplyRandomTeamRoles = false;

	/** Seamless Travel 후 역할 복원용 (TeamNumber, bIsLowerBody, ConnectedPlayerIndex) */
	TMap<FString, TTuple<int32, bool, int32>> PendingRoleRestoreByName;
	/** 인덱스 폴백용 */
	TArray<TTuple<int32, bool, int32>> PendingRoleRestoreByIndex;

	/** PIE 종료 시 월드 GC 방해 방지: OnStart에서 설정한 타이머 핸들 (Shutdown에서 명시적으로 클리어) */
	FTimerHandle ListenServerTimerHandle;
	FTimerHandle SessionRecreateTimerHandle;

	/** OnWorldCleanup 등록 해제용 (Shutdown에서 Remove) */
	FDelegateHandle OnWorldCleanupHandle;

#if WITH_EDITOR
	/** PIE 종료 시 엔진 참조 검사보다 먼저 정리하기 위한 PrePIEEnded 핸들 */
	FDelegateHandle PrePIEEndedHandle;
#endif
};

