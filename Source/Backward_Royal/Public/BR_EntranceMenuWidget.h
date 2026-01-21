// BR_EntranceMenuWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BR_EntranceMenuWidget.generated.h"

class ABRPlayerController;

/**
 * 입장 메뉴 위젯 베이스 클래스
 * 블루프린트 WBP_EntranceMenu1에서 상속받아 사용
 */
UCLASS()
class BACKWARD_ROYAL_API UBR_EntranceMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBR_EntranceMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;

public:
	// 방 생성 함수
	UFUNCTION(BlueprintCallable, Category = "Session")
	void CreateRoom(const FString& RoomName);

	// 방 찾기 함수
	UFUNCTION(BlueprintCallable, Category = "Session")
	void FindRooms();

	// PlayerController 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	ABRPlayerController* GetBRPlayerController() const;

	// 방 생성 완료 이벤트 (블루프린트에서 바인딩 가능)
	UFUNCTION(BlueprintImplementableEvent, Category = "Events")
	void OnCreateRoomComplete(bool bWasSuccessful);

	// 방 찾기 완료 이벤트 (블루프린트에서 바인딩 가능)
	UFUNCTION(BlueprintImplementableEvent, Category = "Events")
	void OnFindRoomsComplete();

private:
	// PlayerController 캐시 (mutable로 선언하여 const 함수에서도 수정 가능)
	UPROPERTY()
	mutable ABRPlayerController* CachedPlayerController;

	// 이벤트 바인딩 해제를 위한 함수
	UFUNCTION()
	void HandleCreateRoomComplete(bool bWasSuccessful);
};
