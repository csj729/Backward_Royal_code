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
	bIsLowerBody = true; // 기본값은 하체
	ConnectedPlayerIndex = -1; // 기본값은 연결 없음
	UserUID = TEXT("");
}

void ABRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRPlayerState, TeamNumber);
	DOREPLIFETIME(ABRPlayerState, bIsHost);
	DOREPLIFETIME(ABRPlayerState, bIsReady);
	DOREPLIFETIME(ABRPlayerState, bIsLowerBody);
	DOREPLIFETIME(ABRPlayerState, ConnectedPlayerIndex);
	DOREPLIFETIME(ABRPlayerState, UserUID);
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
		bIsLowerBody = bLowerBody;
		ConnectedPlayerIndex = ConnectedIndex;
		FString PlayerName = GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = TEXT("Unknown Player");
		}
		FString RoleName = bLowerBody ? TEXT("하체") : TEXT("상체");
		UE_LOG(LogTemp, Log, TEXT("[플레이어 역할] %s: %s 역할 할당 (연결된 플레이어 인덱스: %d)"), 
			*PlayerName, *RoleName, ConnectedIndex);
		OnRep_PlayerRole();
	}
}

void ABRPlayerState::OnRep_PlayerRole()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::SwapControlWithPartner()
{
	if (!HasAuthority()) return;

	ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
	if (!GS || !GS->PlayerArray.IsValidIndex(ConnectedPlayerIndex)) return;

	ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(GS->PlayerArray[ConnectedPlayerIndex]);
	ABRPlayerController* MyPC = Cast<ABRPlayerController>(GetOwner());
	ABRPlayerController* PartnerPC = Cast<ABRPlayerController>(PartnerPS->GetOwner());

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
	FBRUserInfo UserInfo;
	
	UserInfo.UserUID = UserUID;
	UserInfo.PlayerName = GetPlayerName();
	UserInfo.TeamID = TeamNumber;
	UserInfo.bIsHost = bIsHost;
	UserInfo.bIsReady = bIsReady;
	
	// PlayerIndex: 0=하체, 1=상체 (bIsLowerBody를 기반으로 변환)
	UserInfo.PlayerIndex = bIsLowerBody ? 0 : 1;
	
	return UserInfo;
}

void ABRPlayerState::SetUserUID(const FString& NewUserUID)
{
	if (HasAuthority())
	{
		UserUID = NewUserUID;
		UE_LOG(LogTemp, Log, TEXT("[UserUID 설정] %s: UserUID = %s"), *GetPlayerName(), *UserUID);
		OnRep_UserUID();
	}
}

void ABRPlayerState::OnRep_UserUID()
{
	// UI 업데이트를 위한 이벤트 발생 가능
}

void ABRPlayerState::SetPlayerNameString(const FString& NewPlayerName)
{
	if (HasAuthority())
	{
		SetPlayerName(NewPlayerName);
		UE_LOG(LogTemp, Log, TEXT("[플레이어 이름 설정] %s"), *NewPlayerName);
	}
}