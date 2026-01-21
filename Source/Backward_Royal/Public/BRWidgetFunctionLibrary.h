// BRWidgetFunctionLibrary.h
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BRWidgetFunctionLibrary.generated.h"

class ABRPlayerController;
class ABRGameSession;
class ABRGameState;
class ABRPlayerState;
class UUserWidget;

/**
 * 블루프린트 위젯에서 서버 함수를 호출하기 위한 Function Library
 * 상속 변경 없이 블루프린트에서 사용 가능
 */
UCLASS()
class BACKWARD_ROYAL_API UBRWidgetFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================
	// PlayerController 관련 함수들
	// ============================================
	
	// PlayerController 가져오기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|PlayerController", meta = (WorldContext = "WorldContextObject"))
	static ABRPlayerController* GetBRPlayerController(const UObject* WorldContextObject);

	// 방 생성
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject"))
	static void CreateRoom(const UObject* WorldContextObject, const FString& RoomName);

	// 방 찾기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject"))
	static void FindRooms(const UObject* WorldContextObject);

	// 방 참가
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Session", meta = (WorldContext = "WorldContextObject"))
	static void JoinRoom(const UObject* WorldContextObject, int32 SessionIndex);

	// 준비 상태 토글
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Room", meta = (WorldContext = "WorldContextObject"))
	static void ToggleReady(const UObject* WorldContextObject);

	// 랜덤 팀 배정 (방장만)
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Team", meta = (WorldContext = "WorldContextObject"))
	static void RandomTeams(const UObject* WorldContextObject);

	// 플레이어 팀 변경 (방장만)
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Team", meta = (WorldContext = "WorldContextObject"))
	static void ChangeTeam(const UObject* WorldContextObject, int32 PlayerIndex, int32 TeamNumber);

	// 게임 시작 (방장만)
	UFUNCTION(BlueprintCallable, Category = "BR Widget|Game", meta = (WorldContext = "WorldContextObject"))
	static void StartGame(const UObject* WorldContextObject);

	// ============================================
	// GameSession 관련 함수들
	// ============================================
	
	// GameSession 가져오기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|GameSession", meta = (WorldContext = "WorldContextObject"))
	static ABRGameSession* GetBRGameSession(const UObject* WorldContextObject);

	// ============================================
	// GameState 관련 함수들
	// ============================================
	
	// GameState 가져오기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|GameState", meta = (WorldContext = "WorldContextObject"))
	static ABRGameState* GetBRGameState(const UObject* WorldContextObject);

	// ============================================
	// PlayerState 관련 함수들
	// ============================================
	
	// PlayerState 가져오기
	UFUNCTION(BlueprintCallable, Category = "BR Widget|PlayerState", meta = (WorldContext = "WorldContextObject"))
	static ABRPlayerState* GetBRPlayerState(const UObject* WorldContextObject);

	// 방장 여부 확인
	UFUNCTION(BlueprintCallable, Category = "BR Widget|PlayerState", meta = (WorldContext = "WorldContextObject"))
	static bool IsHost(const UObject* WorldContextObject);

	// 준비 상태 확인
	UFUNCTION(BlueprintCallable, Category = "BR Widget|PlayerState", meta = (WorldContext = "WorldContextObject"))
	static bool IsReady(const UObject* WorldContextObject);
};
