// BRPlayerState.cpp
#include "BRPlayerState.h"
#include "BRGameState.h"
#include "BRPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UpperBodyPawn.h"
#include "PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

ABRPlayerState::ABRPlayerState()
{
	TeamNumber = 0;
	bIsHost = false;
	bIsReady = false;
	bIsSpectatorSlot = false;
	bIsLowerBody = true; // 기본값은 하체
	ConnectedPlayerIndex = -1; // 기본값은 연결 없음
	PartnerPlayerState = nullptr;
	UserUID = TEXT("");
}

void ABRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRPlayerState, TeamNumber);
	DOREPLIFETIME(ABRPlayerState, bIsHost);
	DOREPLIFETIME(ABRPlayerState, bIsReady);
	DOREPLIFETIME(ABRPlayerState, bIsSpectatorSlot);
	DOREPLIFETIME(ABRPlayerState, bIsLowerBody);
	DOREPLIFETIME(ABRPlayerState, ConnectedPlayerIndex);
	DOREPLIFETIME(ABRPlayerState, PartnerPlayerState);
	DOREPLIFETIME(ABRPlayerState, UserUID);
	DOREPLIFETIME(ABRPlayerState, CustomizationData);
	DOREPLIFETIME(ABRPlayerState, CurrentStatus);
}

void ABRPlayerState::ServerSetCustomizationData_Implementation(const FBRCustomizationData& NewData)
{
	CustomizationData = NewData;
	// 서버에서도 적용이 필요하면 여기서 델리게이트를 호출하거나 로직 수행
	OnRep_CustomizationData();
}

void ABRPlayerState::OnRep_CustomizationData()
{
	// 데이터가 갱신되었음을 캐릭터에게 알리거나, 캐릭터가 이를 감지하여 외형 갱신
	if (OnCustomizationDataChanged.IsBound())
	{
		OnCustomizationDataChanged.Broadcast();
	}
}

void ABRPlayerState::BeginPlay()
{
	Super::BeginPlay();
}

void ABRPlayerState::SetTeamNumber(int32 NewTeamNumber)
{
	if (HasAuthority())
	{
		int32 OldTeam = TeamNumber;
		TeamNumber = NewTeamNumber;
		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = TEXT("Unknown Player");
		}
		UE_LOG(LogTemp, Log, TEXT("[팀 변경] %s: 팀 %d -> 팀 %d"), *PlayerName, OldTeam, NewTeamNumber);
		OnRep_TeamNumber();
		NotifyUserInfoChanged();
	}
}

void ABRPlayerState::SetIsHost(bool bNewIsHost)
{
	if (HasAuthority())
	{
		bIsHost = bNewIsHost;
		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = TEXT("Unknown Player");
		}
		if (bNewIsHost)
		{
			UE_LOG(LogTemp, Log, TEXT("[방장] %s가 방장이 되었습니다."), *PlayerName);
		}
		OnRep_IsHost();
		NotifyUserInfoChanged();
	}
}

void ABRPlayerState::ToggleReady()
{
	if (HasAuthority())
	{
		bool bWasReady = bIsReady;
		bIsReady = !bIsReady;
		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = TEXT("Unknown Player");
		}
		UE_LOG(LogTemp, Log, TEXT("[준비 상태] %s: %s -> %s"),
			*PlayerName,
			bWasReady ? TEXT("준비 완료") : TEXT("대기 중"),
			bIsReady ? TEXT("준비 완료") : TEXT("대기 중"));
		OnRep_IsReady();
		NotifyUserInfoChanged();

		// 준비 상태 변경 후 게임 시작 가능 여부 확인
		if (UWorld* World = GetWorld())
		{
			if (ABRGameState* BRGameState = World->GetGameState<ABRGameState>())
			{
				BRGameState->CheckCanStartGame();
			}
		}
	}
}

void ABRPlayerState::OnRep_TeamNumber()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::OnRep_IsHost()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::OnRep_IsReady()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::SetPlayerRole(bool bLowerBody, int32 ConnectedIndex)
{
	if (HasAuthority())
	{
		bIsSpectatorSlot = false;
		bIsLowerBody = bLowerBody;
		ConnectedPlayerIndex = ConnectedIndex;
		PartnerPlayerState = nullptr;	

		if (ConnectedIndex >= 0 && GetWorld())
		{
			if (ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>())
			{
				if (GS->PlayerArray.IsValidIndex(ConnectedIndex))
				{
					ABRPlayerState* TargetPartner = Cast<ABRPlayerState>(GS->PlayerArray[ConnectedIndex]);

					if (TargetPartner)
					{
						// [핵심] 서로를 가리키도록 양방향 참조 설정 (Double Linking)

						// 나 -> 파트너
						this->PartnerPlayerState = TargetPartner;

						// 파트너 -> 나 (이 부분이 빠지면 한쪽만 연결됨)
						TargetPartner->PartnerPlayerState = this;
						// 변경사항 즉시 전파
						TargetPartner->OnRep_PartnerPlayerState();
					}
				}
			}
		}

		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = TEXT("Unknown Player");
		}
		FString RoleName = bLowerBody ? TEXT("하체") : TEXT("상체");
		UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: %s 역할 할당 (연결된 플레이어 인덱스: %d)"),
			*PlayerName, *RoleName, ConnectedIndex);
		OnRep_PlayerRole();
		OnRep_PartnerPlayerState();
		NotifyUserInfoChanged();
	}
}

void ABRPlayerState::SetSpectator(bool bSpectator)
{
	if (HasAuthority())
	{
		bIsSpectatorSlot = bSpectator;
		if (bSpectator)
		{
			ConnectedPlayerIndex = -1;
			PartnerPlayerState = nullptr;
			FString PlayerName = GetPlayerName();
			if (PlayerName.IsEmpty()) PlayerName = TEXT("Unknown Player");
			UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: 관전(PlayerIndex 0)으로 설정"), *PlayerName);
		}
		OnRep_PlayerRole();
		NotifyUserInfoChanged();
	}
}

void ABRPlayerState::OnRep_PlayerRole()
{
	// [수정] 역할 정보(상체/하체, 파트너 인덱스 등)가 갱신되면 델리게이트를 방송하여
	// 캐릭터(PlayerCharacter)가 이를 감지하고 파트너 연결을 재시도하도록 함.
	OnPlayerRoleChanged.Broadcast(bIsLowerBody);
}

void ABRPlayerState::OnRep_PartnerPlayerState()
{
	// 로그로 확인 (디버깅용)
	if (PartnerPlayerState)
	{
		UE_LOG(LogTemp, Log, TEXT("[Partner Linked] 나(%s)의 파트너는 %s 입니다."),
			*GetPlayerName(), *PartnerPlayerState->GetPlayerName());
	}

	// 캐릭터에게 커마 다시 적용하라고 알림
	if (APawn* MyPawn = GetPawn())
	{
		if (APlayerCharacter* PC = Cast<APlayerCharacter>(MyPawn))
		{
			// 캐릭터 쪽에서 파트너 포인터를 최우선으로 쓰도록 유도
			PC->OnRep_PlayerState();
		}
	}
}

void ABRPlayerState::SwapControlWithPartner()
{
	if (!HasAuthority()) return;

	ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (!GS || !GS->PlayerArray.IsValidIndex(ConnectedPlayerIndex)) return;

	ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(GS->PlayerArray[ConnectedPlayerIndex]);
	ABRPlayerController* MyPC = Cast<ABRPlayerController>(GetOwningController());
	ABRPlayerController* PartnerPC = Cast<ABRPlayerController>(PartnerPS->GetOwningController());

	if (MyPC && PartnerPC)
	{
		// 1. 현재 각자가 조종 중인 Pawn을 명확히 백업
		APawn* MyOldPawn = MyPC->GetPawn();
		APawn* PartnerOldPawn = PartnerPC->GetPawn();

		if (!MyOldPawn || !PartnerOldPawn) return;

		// 2. 빙의 해제 및 교차 빙의
		MyPC->UnPossess();
		PartnerPC->UnPossess();

		MyPC->Possess(PartnerOldPawn);
		PartnerPC->Possess(MyOldPawn);

		// 3. [핵심] 네트워크 소유권 갱신 (AutonomousProxy 권한 확정)
		PartnerOldPawn->SetOwner(MyPC);
		MyOldPawn->SetOwner(PartnerPC);

		// 4. 상체 부착 상태 재설정
		APlayerCharacter* LowerChar = bIsLowerBody ? Cast<APlayerCharacter>(PartnerOldPawn) : Cast<APlayerCharacter>(MyOldPawn);
		AUpperBodyPawn* UpperPawn = bIsLowerBody ? Cast<AUpperBodyPawn>(MyOldPawn) : Cast<AUpperBodyPawn>(PartnerOldPawn);

		if (UpperPawn && LowerChar)
		{
			UpperPawn->AttachToComponent(LowerChar->HeadMountPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}

		// 5. [가장 중요] 클라이언트에게 입력 시스템 재시작 명령
		// 이 함수가 호출되어야 클라이언트에서 하체의 SetupPlayerInputComponent가 다시 실행됩니다.
		MyPC->ClientRestart(PartnerOldPawn);
		PartnerPC->ClientRestart(MyOldPawn);

		// 6. 설정 적용 람다 실행
		auto ApplyRoleSettings = [this](ABRPlayerController* PC, bool bIsLower)
			{
				if (!PC) return;
				PC->SetupRoleInput(bIsLower);

				APawn* P = PC->GetPawn();
				if (!P) return;

				// 시점 전환 (스크린샷 3번의 시점 고립 해결)
				PC->SetViewTarget(P);

				// 모든 입력 무시 상태 강제 해제
				PC->ResetIgnoreMoveInput();
				PC->SetIgnoreMoveInput(false);
				PC->ResetIgnoreLookInput();
				PC->SetIgnoreLookInput(false);

				if (bIsLower)
				{
					if (ACharacter* Character = Cast<ACharacter>(P))
					{
						Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
					}
				}
			};

		ApplyRoleSettings(MyPC, bIsLowerBody);
		ApplyRoleSettings(PartnerPC, PartnerPS->bIsLowerBody);

		ClientShowSwapAnim();
		PartnerPS->ClientShowSwapAnim();
	}
}

void ABRPlayerState::ClientShowSwapAnim_Implementation()
{
	OnPlayerSwapAnim.Broadcast();
}

FBRUserInfo ABRPlayerState::GetUserInfo() const
{
	// 1. 임시 지역 변수 생성
	FBRUserInfo TempInfo;

	// 2. 기본 정보 취합. PlayerIndex: 0=관전, 1=하체, 2=상체
	TempInfo.UserUID = UserUID;
	TempInfo.PlayerName = GetPlayerName();
	TempInfo.TeamID = TeamNumber;
	TempInfo.bIsHost = bIsHost;
	TempInfo.bIsReady = bIsReady;
	TempInfo.bIsSpectator = bIsSpectatorSlot;
	TempInfo.bIsLowerBody = bIsLowerBody;
	TempInfo.ConnectedPlayerIndex = ConnectedPlayerIndex;
	TempInfo.PlayerIndex = bIsSpectatorSlot ? 0 : (bIsLowerBody ? 1 : 2);

	// 3. 커스터마이징 데이터 취합 (중요!)
	// 클래스 멤버인 CustomizationData를 구조체 필드에 할당
	TempInfo.CustomizationData = CustomizationData;

	// 4. [규칙 준수] 커스텀 로그 매크로 사용
	if (TempInfo.PlayerName.IsEmpty() || TempInfo.PlayerName == TempInfo.UserUID)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetUserInfo | 이름이 비어있거나 UID와 동일함 (Name: %s, UID: %s)"),
			*TempInfo.PlayerName, *TempInfo.UserUID);
	}

	return TempInfo;
}

void ABRPlayerState::SetUserUID(const FString& NewUserUID)
{
	if (HasAuthority())
	{
		UserUID = NewUserUID;
		UE_LOG(LogTemp, Log, TEXT("[UserUID 설정] %s: UserUID = %s"), *GetPlayerName(), *UserUID);
		OnRep_UserUID();
		NotifyUserInfoChanged();
	}
}

void ABRPlayerState::OnRep_UserUID()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::NotifyUserInfoChanged()
{
	if (!HasAuthority()) return;
	UWorld* World = GetWorld();
	if (!World) return;
	ABRGameState* GS = World->GetGameState<ABRGameState>();
	if (GS)
	{
		GS->UpdatePlayerList();
	}
}

void ABRPlayerState::SetPlayerNameString(const FString& NewPlayerName)
{
	if (!HasAuthority()) return;

	// UID를 이름으로 저장하지 않음. 표시용은 "Player N" 등으로 대체됨.
	FString NameToSet = NewPlayerName;
	if (!NameToSet.IsEmpty() && NameToSet == UserUID)
	{
		NameToSet = TEXT("Player");
		UE_LOG(LogTemp, Log, TEXT("[플레이어 이름] UID와 동일하여 'Player'로 대체 저장"));
	}
	SetPlayerName(NameToSet);
	UE_LOG(LogTemp, Log, TEXT("[플레이어 이름 설정] %s"), *NameToSet);
	NotifyUserInfoChanged();
}

void ABRPlayerState::SetPlayerStatus(EPlayerStatus NewStatus)
{
	if (HasAuthority())
	{
		CurrentStatus = NewStatus;
		OnRep_PlayerStatus(); // 서버에서도 로직 실행을 위해 직접 호출
	}
}

void ABRPlayerState::OnRep_PlayerStatus()
{
	// 1. 델리게이트 방송 -> UI(생존자 숫자, 팀원 상태창) 갱신
	OnPlayerStatusChanged.Broadcast(CurrentStatus);

	// 2. 로그
	UE_LOG(LogTemp, Log, TEXT("Player %s Status Changed to %d"), *GetPlayerName(), (int32)CurrentStatus);
}

void ABRPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	// 인자로 들어온 PlayerState는 "새로 생성된(다음 레벨의) PlayerState"입니다.
	ABRPlayerState* NewBRPlayerState = Cast<ABRPlayerState>(PlayerState);
	if (NewBRPlayerState)
	{
		// 1. 값(Value) 타입 데이터는 복사 (커스터마이징 정보 등)
		NewBRPlayerState->CustomizationData = CustomizationData;
		NewBRPlayerState->TeamNumber = TeamNumber;
		NewBRPlayerState->bIsHost = bIsHost;
		NewBRPlayerState->bIsReady = bIsReady;
		NewBRPlayerState->bIsLowerBody = bIsLowerBody;
		NewBRPlayerState->UserUID = UserUID;

		// 2. 연결된 인덱스도 복사 (서버가 나중에 이걸 보고 파트너를 다시 찾아줌)
		NewBRPlayerState->ConnectedPlayerIndex = ConnectedPlayerIndex;

		// 3. [핵심 수정] 파트너 포인터는 '이전 레벨의 객체' 주소이므로 절대 복사 금지!
		// nullptr로 초기화해야 안전하며, 이후 로직에서 다시 바인딩됩니다.
		NewBRPlayerState->PartnerPlayerState = nullptr;

		UE_LOG(LogTemp, Log, TEXT("[CopyProperties] Data Copied for %s (Partner Ptr Reset)"), *GetPlayerName());
	}
}