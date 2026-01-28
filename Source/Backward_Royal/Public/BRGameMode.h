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

	// 방 생성 후 이동할 로비 맵 경로. 비어 있으면 현재 맵 유지.
	// 예: /Game/Main/Level/Main_Scene 또는 /Game/Main/Level/Stage/Stage01_Temple
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings", meta = (DisplayName = "로비 맵 경로"))
	FString LobbyMapPath;

	// 게임 시작 맵 경로 (레거시 - 랜덤 맵 선택 시 사용되지 않음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings")
	FString GameMapPath = TEXT("/Game/Main/Level/Stage/Stage01_Temple");

	// Stage 맵 목록 (랜덤 선택용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings")
	TArray<FString> StageMapPaths = {
		TEXT("/Game/Main/Level/Stage/Stage01_Temple"),
		TEXT("/Game/Main/Level/Stage/Stage02_Bushes"),
		TEXT("/Game/Main/Level/Stage/Stage03_Arena")
	};

	// 랜덤 맵 선택 사용 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Settings")
	bool bUseRandomMap = true;

	// 게임 시작
	UFUNCTION(BlueprintCallable, Category = "Game")
	void StartGame();

	// 플레이어 로그인 처리
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 플레이어 로그아웃 처리
	virtual void Logout(AController* Exiting) override;

	// 랜덤 팀 배정 후 상체/하체 Pawn 재배치 (상체 스폰 및 빙의)
	void ApplyRoleChangesForRandomTeams();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 에디터(BP_BRGameMode)에서 BP_UpperBodyPawn을 할당할 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Classes")
	TSubclassOf<class AUpperBodyPawn> UpperBodyClass;
};

