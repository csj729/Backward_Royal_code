// BR_JoinMenuWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BR_JoinMenuWidget.generated.h"

class ABRPlayerController;
class ABRGameSession;

/**
 * 참가 메뉴 위젯 베이스 클래스
 * 블루프린트 WBP_JoinMenu1에서 상속받아 사용
 */
UCLASS()
class BACKWARD_ROYAL_API UBR_JoinMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBR_JoinMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// 방 참가 함수
	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinRoom(int32 SessionIndex);

	// 방 찾기 함수 (새로고침)
	UFUNCTION(BlueprintCallable, Category = "Session")
	void RefreshRoomList();

	// PlayerController 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	ABRPlayerController* GetBRPlayerController() const;

	// GameSession 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	ABRGameSession* GetBRGameSession() const;

	// 방 참가 완료 이벤트 (블루프린트에서 바인딩 가능)
	UFUNCTION(BlueprintImplementableEvent, Category = "Events")
	void OnJoinRoomComplete(bool bWasSuccessful);

	// 방 찾기 완료 이벤트 (블루프린트에서 바인딩 가능)
	UFUNCTION(BlueprintImplementableEvent, Category = "Events")
	void OnFindRoomsComplete();

private:
	// PlayerController 캐시 (mutable로 선언하여 const 함수에서도 수정 가능)
	UPROPERTY()
	mutable ABRPlayerController* CachedPlayerController;

	// GameSession 참조 (mutable로 선언하여 const 함수에서도 수정 가능)
	UPROPERTY()
	mutable ABRGameSession* CachedGameSession;

	// 이벤트 바인딩 해제를 위한 함수
	UFUNCTION()
	void HandleJoinSessionComplete(bool bWasSuccessful);
};
