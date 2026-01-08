// BRGameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WeaponTypes.h"
#include "BRGameInstance.generated.h"

UCLASS()
class BACKWARD_ROYAL_API UBRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UBRGameInstance();
	
	virtual void Init() override;

	// 방 생성
	UFUNCTION(Exec)
	void CreateRoom(const FString& RoomName = TEXT("TestRoom"));

	// 방 찾기
	UFUNCTION(Exec)
	void FindRooms();

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

	// 에디터에서 전역적으로 사용할 무기 데이터 테이블 할당
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	UDataTable* WeaponDataTable;

	// 어디서든 ID로 무기 정보를 가져올 수 있는 함수
	UFUNCTION(BlueprintCallable, Category = "Data")
	bool GetWeaponData(FName RowName, FWeaponData& OutData);
};

