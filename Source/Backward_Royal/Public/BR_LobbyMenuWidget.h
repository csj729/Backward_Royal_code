// BR_LobbyMenuWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BR_LobbyMenuWidget.generated.h"

class ABRPlayerController;
class ABRGameState;
class ABRPlayerState;

/**
 * 로비 메뉴 위젯 베이스 클래스
 * 블루프린트 WBP_LobbyMenu1에서 상속받아 사용
 */
UCLASS()
class BACKWARD_ROYAL_API UBR_LobbyMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBR_LobbyMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
	// 준비 상태 토글
	UFUNCTION(BlueprintCallable, Category = "Room")
	void ToggleReady();

	// 랜덤 팀 배정 (방장만)
	UFUNCTION(BlueprintCallable, Category = "Team")
	void RequestRandomTeams();

	// 플레이어 팀 변경 (방장만)
	UFUNCTION(BlueprintCallable, Category = "Team")
	void ChangePlayerTeam(int32 PlayerIndex, int32 TeamNumber);

	// 게임 시작 (방장만)
	UFUNCTION(BlueprintCallable, Category = "Game")
	void RequestStartGame();

	// PlayerController 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	ABRPlayerController* GetBRPlayerController() const;

	// GameState 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	ABRGameState* GetBRGameState() const;

	// PlayerState 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	ABRPlayerState* GetBRPlayerState() const;

	// 방장 여부 확인
	UFUNCTION(BlueprintCallable, Category = "Room")
	bool IsHost() const;

	// 준비 상태 확인
	UFUNCTION(BlueprintCallable, Category = "Room")
	bool IsReady() const;

	// 플레이어 목록 변경 이벤트 (블루프린트에서 바인딩 가능)
	UFUNCTION(BlueprintImplementableEvent, Category = "Events")
	void OnPlayerListChanged();

	// 팀 변경 이벤트 (블루프린트에서 바인딩 가능)
	UFUNCTION(BlueprintImplementableEvent, Category = "Events")
	void OnTeamChanged();

	// 게임 시작 가능 여부 변경 이벤트 (블루프린트에서 바인딩 가능)
	UFUNCTION(BlueprintImplementableEvent, Category = "Events")
	void OnCanStartGameChanged(bool bCanStart);

private:
	// PlayerController 캐시 (mutable로 선언하여 const 함수에서도 수정 가능)
	UPROPERTY()
	mutable ABRPlayerController* CachedPlayerController;

	// GameState 참조 (mutable로 선언하여 const 함수에서도 수정 가능)
	UPROPERTY()
	mutable ABRGameState* CachedGameState;

	// 이벤트 바인딩 해제를 위한 함수들
	UFUNCTION()
	void HandlePlayerListChanged();

	UFUNCTION()
	void HandleTeamChanged();

	UFUNCTION()
	void HandleCanStartGameChanged();
};
