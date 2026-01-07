// BRGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BRGameMode.generated.h"

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

	// 게임 시작 맵 경로
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings")
	FString GameMapPath = TEXT("/Game/Main/Level/Map_Test1");

	// 게임 시작
	UFUNCTION(BlueprintCallable, Category = "Game")
	void StartGame();

	// 플레이어 로그인 처리
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 플레이어 로그아웃 처리
	virtual void Logout(AController* Exiting) override;

protected:
	virtual void BeginPlay() override;
};

