// BRGameState.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BRUserInfo.h"
#include "BRGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerListChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamChanged);

UCLASS()
class BACKWARD_ROYAL_API ABRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABRGameState();

	// 최소 플레이어 수
	UPROPERTY(BlueprintReadOnly, Category = "Room Settings")
	int32 MinPlayers = 4;

	// 최대 플레이어 수
	UPROPERTY(BlueprintReadOnly, Category = "Room Settings")
	int32 MaxPlayers = 8;

	// 현재 플레이어 수
	UPROPERTY(ReplicatedUsing = OnRep_PlayerCount, BlueprintReadOnly, Category = "Room")
	int32 PlayerCount;

	// 게임 시작 가능 여부
	UPROPERTY(ReplicatedUsing = OnRep_CanStartGame, BlueprintReadOnly, Category = "Room")
	bool bCanStartGame;

	// 플레이어 목록 변경 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerListChanged OnPlayerListChanged;

	// 팀 변경 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTeamChanged OnTeamChanged;

	// 플레이어 목록 업데이트
	UFUNCTION(BlueprintCallable, Category = "Room")
	void UpdatePlayerList();

	// 모든 플레이어의 UserInfo 배열 가져오기 (UI에서 사용)
	UFUNCTION(BlueprintCallable, Category = "Room")
	TArray<FBRUserInfo> GetAllPlayerUserInfo() const;

	// 특정 플레이어의 UserInfo 가져오기
	UFUNCTION(BlueprintCallable, Category = "Room")
	FBRUserInfo GetPlayerUserInfo(int32 PlayerIndex) const;

	// 게임 시작 가능 여부 확인
	UFUNCTION(BlueprintCallable, Category = "Room")
	void CheckCanStartGame();

	// 랜덤 팀 배정
	UFUNCTION(BlueprintCallable, Category = "Team")
	void AssignRandomTeams();

	// 모든 플레이어가 준비되었는지 확인
	UFUNCTION(BlueprintCallable, Category = "Room")
	bool AreAllPlayersReady() const;
	
	// 호스트를 제외한 모든 플레이어가 준비되었는지 확인
	UFUNCTION(BlueprintCallable, Category = "Room")
	bool AreAllNonHostPlayersReady() const;

	// 플레이어 수 변경 시 호출
	UFUNCTION()
	void OnRep_PlayerCount();

	// 게임 시작 가능 여부 변경 시 호출
	UFUNCTION()
	void OnRep_CanStartGame();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
};

