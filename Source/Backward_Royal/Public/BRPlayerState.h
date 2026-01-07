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

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
};

