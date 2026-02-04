// BRPlayerState.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BRUserInfo.h"
#include "CustomizationInfo.h"
#include "BRPlayerState.generated.h"

UENUM(BlueprintType)
enum class EPlayerStatus : uint8
{
	Alive       UMETA(DisplayName = "Alive"),		// 생존
	Dead        UMETA(DisplayName = "Dead"),		// 사망
	Spectating  UMETA(DisplayName = "Spectating")	// 관전
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerRoleChanged, bool, bIsLowerBody);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStatusChanged, EPlayerStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerSwapAnim);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCustomizationDataChanged);

UCLASS()
class BACKWARD_ROYAL_API ABRPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ABRPlayerState();

	// 팀 번호 (0 = 팀 없음, 1 = 팀 1, 2 = 팀 2, ...)
	UPROPERTY(ReplicatedUsing = OnRep_TeamNumber, BlueprintReadOnly, Category = "Team")
	int32 TeamNumber;

	// 방장 여부
	UPROPERTY(ReplicatedUsing = OnRep_IsHost, BlueprintReadOnly, Category = "Room")
	bool bIsHost;

	// 준비 상태
	UPROPERTY(ReplicatedUsing = OnRep_IsReady, BlueprintReadWrite, Category = "Room")
	bool bIsReady;

	// 하체 역할 여부 (true = 하체, false = 상체)
	UPROPERTY(ReplicatedUsing = OnRep_PlayerRole, BlueprintReadOnly, Category = "Player Role")
	bool bIsLowerBody;

	// 연결된 플레이어 인덱스
	// 하체인 경우: 연결된 상체 플레이어의 인덱스 (-1이면 아직 없음)
	// 상체인 경우: 연결된 하체 플레이어의 인덱스
	UPROPERTY(ReplicatedUsing = OnRep_PlayerRole, BlueprintReadOnly, Category = "Player Role")
	int32 ConnectedPlayerIndex;

	// 사용자 고유 ID (예: Steam ID, 계정 ID 등)
	UPROPERTY(ReplicatedUsing = OnRep_UserUID, BlueprintReadWrite, Category = "User Info")
	FString UserUID;

	// 플레이어 정보 구조체 가져오기 (UI에서 사용)
	UFUNCTION(BlueprintCallable, Category = "User Info")
	FBRUserInfo GetUserInfo() const;

	// UserUID 설정
	UFUNCTION(BlueprintCallable, Category = "User Info")
	void SetUserUID(const FString& NewUserUID);

	// UserUID 변경 시 호출되는 이벤트
	UFUNCTION()
	void OnRep_UserUID();

	// 플레이어 이름 설정
	UFUNCTION(BlueprintCallable, Category = "User Info")
	void SetPlayerNameString(const FString& NewPlayerName);

	// 팀 번호 설정
	UFUNCTION(BlueprintCallable, Category = "Team")
	void SetTeamNumber(int32 NewTeamNumber);

	// 방장 설정
	UFUNCTION(BlueprintCallable, Category = "Room")
	void SetIsHost(bool bNewIsHost);

	// 준비 상태 토글
	UFUNCTION(BlueprintCallable, Category = "Room")
	void ToggleReady();

	// 팀 번호 변경 시 호출되는 이벤트
	UFUNCTION()
	void OnRep_TeamNumber();

	// 방장 상태 변경 시 호출되는 이벤트
	UFUNCTION()
	void OnRep_IsHost();

	// 준비 상태 변경 시 호출되는 이벤트
	UFUNCTION()
	void OnRep_IsReady();

	// 플레이어 역할 설정
	UFUNCTION(BlueprintCallable, Category = "Player Role")
	void SetPlayerRole(bool bLowerBody, int32 ConnectedIndex);

	// 플레이어 역할 변경 시 호출되는 이벤트
	UFUNCTION()
	void OnRep_PlayerRole();

	/** [서버 전용] 유저 인포 관련 값이 바뀌었을 때 GameState의 PlayerListForDisplay 갱신 요청 */
	void NotifyUserInfoChanged();

	UPROPERTY(ReplicatedUsing = OnRep_PlayerStatus, BlueprintReadOnly, Category = "Status")
	EPlayerStatus CurrentStatus = EPlayerStatus::Alive;

	UFUNCTION()
	void OnRep_PlayerStatus();	

	// 서버에서 상태 변경 시 호출
	void SetPlayerStatus(EPlayerStatus NewStatus);

	// 상태 변경 시 UI 등에 알릴 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnPlayerStatusChanged OnPlayerStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerRoleChanged OnPlayerRoleChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerSwapAnim OnPlayerSwapAnim;

	UFUNCTION(Client, Reliable)
	void ClientShowSwapAnim();

	void SwapControlWithPartner();

	// ReplicatedUsing으로 변경하여 수신 시 함수 호출 유도
	UPROPERTY(ReplicatedUsing = OnRep_CustomizationData, BlueprintReadOnly, Category = "Customization")
	FBRCustomizationData CustomizationData;

	// 구독 가능한 이벤트 디스패처
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnCustomizationDataChanged OnCustomizationDataChanged;

	// 서버에 내 커마 정보를 알리는 함수
	UFUNCTION(Server, Reliable)
	void ServerSetCustomizationData(const FBRCustomizationData& NewData);

	UFUNCTION()
	void OnRep_CustomizationData();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
};

