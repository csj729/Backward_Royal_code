// BRCheatManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "BRCheatManager.generated.h"

UCLASS()
class BACKWARD_ROYAL_API UBRCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UBRCheatManager();

	// 방 생성
	UFUNCTION(Exec)
	void CreateRoom(const FString& RoomName = TEXT("TestRoom"));

	// 방 찾기
	UFUNCTION(Exec)
	void FindRooms();

	// 방 참가
	UFUNCTION(Exec)
	void JoinRoom(int32 SessionIndex);

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

	/** Listen Server로 재시작 (맵?listen). 호스트는 이 명령 실행 후 방 만들기. 콘솔: OpenListenServer */
	UFUNCTION(Exec, Category = "Session")
	void OpenListenServer();

	/** [부하 테스트] 서버 상태 요약 출력 (연결 인원, 맵, stat 명령 안내). 서버/호스트 콘솔에서 StatServer */
	UFUNCTION(Exec, Category = "Session")
	void StatServer();
};

