// BRPlayerState.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BRPlayerState.generated.h"

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

	void SwapControlWithPartner();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
};

