#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "TimerManager.h"
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

	// 찾은 세션 개수 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	int32 GetSessionCount() const;

	// 특정 인덱스의 세션 이름 가져오기
	UFUNCTION(BlueprintCallable, Category = "Session")
	FString GetSessionName(int32 SessionIndex) const;

	// 세션이 생성되어 있는지 확인 (블루프린트에서 사용 가능)
	UFUNCTION(BlueprintCallable, Category = "Session")
	bool HasActiveSession() const;

	/** PIE 종료 시 GameInstance::Shutdown에서 호출. SessionInterface 델리게이트 및 PendingRoom 타이머를 먼저 해제해 월드 참조 사슬을 끊음 */
	void UnbindSessionDelegatesForPIEExit();

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

	// 방 찾기 0건 시 자동 재시도 (간헐적 실패 완화)
	int32 FindSessionsRetryCount;
	FTimerHandle FindSessionsRetryHandle;
	static constexpr int32 MaxFindSessionsRetries = 2;

	/** BeginPlay에서 PendingRoomName 자동 생성 시 사용. 로컬 핸들로 두면 PIE 종료 시 정리되지 않아 월드 참조 남음 → 멤버로 두고 EndPlay/Unbind에서 클리어 */
	FTimerHandle PendingCreateRoomTimerHandle;

	// DestroySession 완료 후 CreateSession 호출을 위한 방 이름 저장
	FString PendingRoomName;
	bool bPendingCreateSession;

	/** NGDA 스타일: CreateSession 성공 후 StartSession → ServerTravel(TravelURL) 호출 시 사용 */
	FString TravelURL;

	// Online Subsystem 초기화
	void InitializeOnlineSubsystem();

	/** 방 생성 시 사용할 리슨 서버 URL 구성 (LobbyMapPath 또는 현재 맵 + ?listen) */
	FString BuildTravelURL() const;

	// 세션 생성 완료 콜백
	void OnCreateSessionCompleteDelegate(FName InSessionName, bool bWasSuccessful);

	/** NGDA 스타일: StartSession 완료 시 호출. 성공 시 ServerTravel(TravelURL) 실행 */
	void OnStartSessionCompleteDelegate(FName InSessionName, bool bWasSuccessful);

	// 세션 제거 완료 콜백
	void OnDestroySessionCompleteDelegate(FName InSessionName, bool bWasSuccessful);

	// 세션 찾기 완료 콜백
	void OnFindSessionsCompleteDelegate(bool bWasSuccessful);

	// 세션 참가 완료 콜백
	void OnJoinSessionCompleteDelegate(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);

	// 방 참가 (C++ 전용)
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);

	void FindSessionsInternal(bool bIsRetry);
	void FindSessionsRetryCallback();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
