#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "BRGameSession.generated.h"

// Forward declarations
class FOnlineSessionSearchResult;
class FOnlineSessionSearch;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBRCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBRFindSessionsComplete, const TArray<FOnlineSessionSearchResult>&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBRJoinSessionComplete, bool, bWasSuccessful);
// 블루프린트용 방 찾기 완료 이벤트 (세션 개수 전달)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBRFindSessionsCompleteBP, int32, SessionCount);

UCLASS()
class BACKWARD_ROYAL_API ABRGameSession : public AGameSession
{
	GENERATED_BODY()

public:
	ABRGameSession();

	// 방 생성
	UFUNCTION(BlueprintCallable, Category = "Session")
	void CreateRoomSession(const FString& RoomName);

	// 방 찾기
	UFUNCTION(BlueprintCallable, Category = "Session")
	void FindSessions();

	// 방 참가 (인덱스로)
	UFUNCTION(BlueprintCallable, Category = "Session")
	void JoinSessionByIndex(int32 SessionIndex);

	// 방 참가 (C++ 전용, Blueprint에서 사용 불가)
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);

	// 찾은 세션 개수 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	int32 GetSessionCount() const;

	// 특정 인덱스의 세션 이름 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	FString GetSessionName(int32 SessionIndex) const;

	// 세션이 생성되어 있는지 확인 (블루프린트에서 사용 가능)
	UFUNCTION(BlueprintCallable, Category = "Session")
	bool HasActiveSession() const;

	// 방 생성 완료 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBRCreateSessionComplete OnCreateSessionComplete;

	// 방 찾기 완료 이벤트 (C++ 전용, Blueprint에서 사용 불가)
	FOnBRFindSessionsComplete OnFindSessionsComplete;

	// 방 찾기 완료 이벤트 (블루프린트용)
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBRFindSessionsCompleteBP OnFindSessionsCompleteBP;

	// 방 참가 완료 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnBRJoinSessionComplete OnJoinSessionComplete;

protected:
	// Online Subsystem 세션 인터페이스
	IOnlineSessionPtr SessionInterface;

	// 세션 설정
	TSharedPtr<FOnlineSessionSettings> SessionSettings;

	// 세션 검색 설정
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	// 검색 진행 중 플래그
	bool bIsSearchingSessions;

	// 세션 생성 완료 콜백
	void OnCreateSessionCompleteDelegate(FName InSessionName, bool bWasSuccessful);

	// 세션 찾기 완료 콜백
	void OnFindSessionsCompleteDelegate(bool bWasSuccessful);

	// 세션 참가 완료 콜백
	void OnJoinSessionCompleteDelegate(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);

	// Online Subsystem 초기화 (BeginPlay에서 호출)
	void InitializeOnlineSubsystem();

	virtual void BeginPlay() override;
};

