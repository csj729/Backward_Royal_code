// BRUserInfo.h
#pragma once

#include "CoreMinimal.h"
#include "BRUserInfo.generated.h"

/**
 * 플레이어 정보 구조체
 * UI에서 사용하기 위한 플레이어 정보를 담는 구조체
 */
USTRUCT(BlueprintType)
struct BACKWARD_ROYAL_API FBRUserInfo
{
	GENERATED_BODY()

	// 사용자 고유 ID (예: Steam ID, 계정 ID 등)
	UPROPERTY(BlueprintReadWrite, Category = "User Info")
	FString UserUID;

	// 플레이어 이름
	UPROPERTY(BlueprintReadWrite, Category = "User Info")
	FString PlayerName;

	// 팀 ID (0 = 팀 없음, 1 = 팀 1, 2 = 팀 2, ...)
	UPROPERTY(BlueprintReadWrite, Category = "User Info")
	int32 TeamID;

	// 플레이어 인덱스 (GameState의 PlayerArray에서의 인덱스)
	UPROPERTY(BlueprintReadWrite, Category = "User Info")
	int32 PlayerIndex;

	// 방장 여부
	UPROPERTY(BlueprintReadWrite, Category = "User Info")
	bool bIsHost;

	// 준비 상태
	UPROPERTY(BlueprintReadWrite, Category = "User Info")
	bool bIsReady;

	FBRUserInfo()
		: UserUID(TEXT(""))
		, PlayerName(TEXT(""))
		, TeamID(0)
		, PlayerIndex(-1)
		, bIsHost(false)
		, bIsReady(false)
	{
	}
};

/** 로비/UI 표시용: PlayerName이 비어있거나 UserUID와 같으면 fallback(Player N) 사용 */
BACKWARD_ROYAL_API bool ShouldUseFallbackDisplayName(const FString& PlayerName, const FString& UserUID);
