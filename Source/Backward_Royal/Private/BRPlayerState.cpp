// Source/Backward_Royal/Private/BRPlayerState.cpp

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
	PartnerPlayerState = nullptr; // [신규] 포인터 초기화
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
	DOREPLIFETIME(ABRPlayerState, ConnectedPlayerIndex); // [호환성] 인덱스 유지
	DOREPLIFETIME(ABRPlayerState, PartnerPlayerState);   // [핵심] 파트너 포인터 복제
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
	// 데이터가 갱신되었음을 알림
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

// [수정] 인덱스 기반 역할 설정 -> 포인터 버전으로 위임
void ABRPlayerState::SetPlayerRole(bool bLowerBody, int32 ConnectedIndex)
{
	if (HasAuthority())
	{
		ABRPlayerState* FoundPartner = nullptr;
		if (ConnectedIndex >= 0)
		{
			if (AGameStateBase* GS = GetWorld()->GetGameState())
			{
				if (GS->PlayerArray.IsValidIndex(ConnectedIndex))
				{
					FoundPartner = Cast<ABRPlayerState>(GS->PlayerArray[ConnectedIndex]);
				}
			}
		}

		// 실제 로직은 포인터 기반 함수에서 처리
		SetPlayerRole(bLowerBody, FoundPartner);
	}
}

// [신규] 포인터 기반 역할 설정 (메인 로직)
void ABRPlayerState::SetPlayerRole(bool bLowerBody, ABRPlayerState* NewPartner)
{
	if (HasAuthority())
	{
		bIsSpectatorSlot = false;
		bIsLowerBody = bLowerBody;

		// 1. 포인터 설정 (핵심)
		SetPartner(NewPartner);

		// 2. 인덱스 자동 갱신 (호환성 유지)
		if (NewPartner)
		{
			if (AGameStateBase* GS = GetWorld()->GetGameState())
			{
				ConnectedPlayerIndex = GS->PlayerArray.Find(NewPartner);
			}
		}
		else
		{
			ConnectedPlayerIndex = -1;
		}

		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty()) PlayerName = TEXT("Unknown Player");
		FString RoleName = bLowerBody ? TEXT("하체") : TEXT("상체");
		FString PartnerName = NewPartner ? NewPartner->GetPlayerName() : TEXT("None");

		UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: %s 역할 할당 (파트너: %s, 인덱스: %d)"),
			*PlayerName, *RoleName, *PartnerName, ConnectedPlayerIndex);

		OnRep_PlayerRole();
		NotifyUserInfoChanged();
	}
}

// [신규] 파트너 직접 설정
void ABRPlayerState::SetPartner(ABRPlayerState* NewPartner)
{
	if (HasAuthority())
	{
		PartnerPlayerState = NewPartner;
		OnRep_PartnerPlayerState(); // 서버에서도 로직 수행을 위해 호출
	}
}

// [신규] 파트너 정보 수신 시 호출
void ABRPlayerState::OnRep_PartnerPlayerState()
{
	// 캐릭터에게 파트너 정보가 갱신되었음을 알림 (커스터마이징 적용 트리거)
	if (APawn* MyPawn = GetPawn())
	{
		if (APlayerCharacter* PC = Cast<APlayerCharacter>(MyPawn))
		{
			PC->OnRep_PlayerState();
		}
	}
}

void ABRPlayerState::SetSpectator(bool bSpectator)
{
	if (HasAuthority())
	{
		bIsSpectatorSlot = bSpectator;
		if (bSpectator)
		{
			SetPartner(nullptr); // 파트너 없음
			ConnectedPlayerIndex = -1;
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
	// UI 업데이트를 위한 이벤트 발생 가능
}

// [수정] 스왑 로직 (PartnerPlayerState 우선 사용)
void ABRPlayerState::SwapControlWithPartner()
{
	// 1. 서버 권한 확인
	if (!HasAuthority()) return;

	ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	// ConnectedPlayerIndex 유효성 체크
	if (!GS || !GS->PlayerArray.IsValidIndex(ConnectedPlayerIndex)) return;

	// 파트너와 내 컨트롤러 가져오기
	ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(GS->PlayerArray[ConnectedPlayerIndex]);
	ABRPlayerController* MyPC = Cast<ABRPlayerController>(GetOwningController());
	ABRPlayerController* PartnerPC = Cast<ABRPlayerController>(PartnerPS ? PartnerPS->GetOwningController() : nullptr);

	if (MyPC && PartnerPC)
	{
		// [1] 스왑 전, 현재 각자가 조종 중인 Pawn 백업
		APawn* MyOldPawn = MyPC->GetPawn();
		APawn* PartnerOldPawn = PartnerPC->GetPawn();

		if (!MyOldPawn || !PartnerOldPawn) return;

		// [2] 빙의 해제 (UnPossess)
		MyPC->UnPossess();
		PartnerPC->UnPossess();

		// [3] 교차 빙의 (Possess)
		// 주의: MyOldPawn은 내가 조종하던 것이므로, 이제 파트너가 조종해야 함
		MyPC->Possess(PartnerOldPawn);
		PartnerPC->Possess(MyOldPawn);

		// [4] 네트워크 소유권 갱신 (AutonomousProxy 권한 확정)
		// Possess 내부에서 SetOwner를 하긴 하지만, 멀티플레이어 환경에서 확실한 동기화를 위해 명시
		PartnerOldPawn->SetOwner(MyPC);
		MyOldPawn->SetOwner(PartnerPC);

		// [5] 상체/하체 부착 관계 재설정
		// bIsLowerBody는 이미 SwitchOrb에서 반전된 상태임 (현재 나의 역할)

		// 내가 하체라면 -> 파트너가 타던거(PartnerOldPawn)가 하체 캐릭터, 내가 타던거(MyOldPawn)가 상체
		APlayerCharacter* LowerChar = nullptr;
		AUpperBodyPawn* UpperPawn = nullptr;

		if (bIsLowerBody)
		{
			LowerChar = Cast<APlayerCharacter>(PartnerOldPawn); // 내가 새로 탄 게 탱크
			UpperPawn = Cast<AUpperBodyPawn>(MyOldPawn);       // 내가 내린 게 포탑
		}
		else
		{
			LowerChar = Cast<APlayerCharacter>(MyOldPawn);      // 내가 내린 게 탱크
			UpperPawn = Cast<AUpperBodyPawn>(PartnerOldPawn);   // 내가 새로 탄 게 포탑
		}

		if (UpperPawn && LowerChar)
		{
			// 상체를 하체에 다시 부착 (소유권 변경 후 재부착해야 깔끔함)
			UpperPawn->AttachToComponent(LowerChar->HeadMountPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}

		// [6] 클라이언트 입력 시스템 재시작 (RPC)
		// 클라이언트의 PlayerController가 새 Pawn에 대해 SetupInputComponent를 호출하게 함
		MyPC->ClientRestart(MyPC->GetPawn());
		PartnerPC->ClientRestart(PartnerPC->GetPawn());

		// [7] 역할별 설정 적용 람다 (회전값 초기화 로직 추가됨)
		auto ApplyRoleSettings = [](ABRPlayerController* PC, bool bIsLower, APawn* ControlledPawn)
			{
				if (!PC || !ControlledPawn) return;

				// 1. 입력 매핑 컨텍스트 변경
				PC->SetupRoleInput(bIsLower);

				// 2. 시점 타겟 확실하게 설정
				PC->SetViewTarget(ControlledPawn);

				// 3. 입력 무시 상태 해제
				PC->ResetIgnoreMoveInput();
				PC->SetIgnoreMoveInput(false);
				PC->ResetIgnoreLookInput();
				PC->SetIgnoreLookInput(false);

				// 4. [중요 수정] 회전값(Control Rotation) 초기화
				// 스왑 직후 카메라가 엉뚱한 곳을 보거나 꼬이는 현상 방지
				if (bIsLower)
				{
					// 하체(탱크)를 맡은 사람은 탱크의 진행 방향을 바라보게 함
					PC->SetControlRotation(ControlledPawn->GetActorRotation());

					// 이동 모드 걷기로 초기화
					if (ACharacter* Char = Cast<ACharacter>(ControlledPawn))
					{
						Char->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
					}
				}
				else
				{
					// 상체(포탑)를 맡은 사람은 탱크의 등 뒤(180도)를 바라보게 함
					// (UpperBodyPawn은 하체에 붙어있으므로 부모의 회전을 가져옴)
					if (AActor* Parent = ControlledPawn->GetAttachParentActor())
					{
						FRotator NewRot = Parent->GetActorRotation();
						NewRot.Yaw += 180.0f; // 탱크 정면 기준 뒤쪽
						NewRot.Pitch = 0.0f;
						PC->SetControlRotation(NewRot);
					}
				}
			};

		// 람다 호출
		ApplyRoleSettings(MyPC, bIsLowerBody, MyPC->GetPawn());
		ApplyRoleSettings(PartnerPC, PartnerPS->bIsLowerBody, PartnerPC->GetPawn());

		// [8] UI/애니메이션 연출
		ClientShowSwapAnim();
		if (PartnerPS)
		{
			PartnerPS->ClientShowSwapAnim();
		}
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
	TempInfo.ConnectedPlayerIndex = ConnectedPlayerIndex; // 인덱스 유지
	TempInfo.PlayerIndex = bIsSpectatorSlot ? 0 : (bIsLowerBody ? 1 : 2);

	// 3. 커스터마이징 데이터 취합
	TempInfo.CustomizationData = CustomizationData;

	// 4. 로그
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

	// 새로 생성된 PlayerState로 형변환
	ABRPlayerState* NewBRPlayerState = Cast<ABRPlayerState>(PlayerState);
	if (NewBRPlayerState)
	{
		// [중요] 기존 데이터를 새 PlayerState로 복사
		NewBRPlayerState->CustomizationData = CustomizationData; // 커스터마이징 정보 유지
		NewBRPlayerState->TeamNumber = TeamNumber;               // 팀 번호 유지
		NewBRPlayerState->bIsHost = bIsHost;                     // 방장 권한 유지
		NewBRPlayerState->bIsReady = bIsReady;                   // 준비 상태 유지
		NewBRPlayerState->bIsLowerBody = bIsLowerBody;           // 역할 유지
		NewBRPlayerState->UserUID = UserUID;                     // UID 유지
		NewBRPlayerState->ConnectedPlayerIndex = ConnectedPlayerIndex; // 연결 인덱스 유지

		// [추가] 파트너 포인터 복사
		NewBRPlayerState->PartnerPlayerState = PartnerPlayerState;

		UE_LOG(LogTemp, Log, TEXT("[CopyProperties] %s의 데이터가 다음 레벨로 복사되었습니다."), *GetPlayerName());
	}
}