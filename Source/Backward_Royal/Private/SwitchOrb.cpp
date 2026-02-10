// SwitchOrb.cpp
#include "SwitchOrb.h"
#include "Components/SphereComponent.h"
#include "PlayerCharacter.h"
#include "UpperBodyPawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BRPlayerState.h"
#include "BRGameState.h"

ASwitchOrb::ASwitchOrb()
{
    // 1. 충돌체 설정
    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    RootComponent = CollisionSphere;
    CollisionSphere->SetSphereRadius(100.0f);
    CollisionSphere->SetCollisionProfileName(TEXT("Trigger"));

    // 2. 나이아가라 컴포넌트 설정
    OrbNiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("OrbNiagaraComp"));
    OrbNiagaraComp->SetupAttachment(RootComponent);

    bReplicates = true;
}

void ASwitchOrb::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 오버랩 이벤트를 바인딩
    if (HasAuthority())
    {
        CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ASwitchOrb::OnOrbOverlap);
    }
}

void ASwitchOrb::OnOrbOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 1. 권한 및 대상 확인
    if (!HasAuthority())
    {
        // 클라이언트라서 리턴되는 경우는 정상입니다 (서버에서만 처리)
        return;
    }

    if (!OtherActor) return;

    // [개선] 플레이어 찾기: PlayerCharacter 직접 또는 UpperBodyPawn의 ParentBody 또는 부모 체인
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(OtherActor);
    ABRPlayerState* MyPS = nullptr;

    // 1) 직접 PlayerCharacter인 경우
    if (PlayerChar)
    {
        MyPS = PlayerChar->GetPlayerState<ABRPlayerState>();
    }
    // 2) UpperBodyPawn인 경우 - 조종하는 컨트롤러의 PlayerState 사용
    else if (AUpperBodyPawn* UpperPawn = Cast<AUpperBodyPawn>(OtherActor))
    {
        if (AController* Controller = UpperPawn->GetController())
        {
            MyPS = Controller->GetPlayerState<ABRPlayerState>();
            // ParentBodyCharacter도 설정되어 있으면 그것을 PlayerChar로 사용
            PlayerChar = UpperPawn->ParentBodyCharacter;
        }
        if (!MyPS || !PlayerChar)
        {
            return;
        }
    }
    // 3) 무기/부착물 등 - Owner/Instigator/부모 체인에서 PlayerCharacter 찾기
    else
    {
        PlayerChar = Cast<APlayerCharacter>(OtherActor->GetOwner());
        if (!PlayerChar)
            PlayerChar = Cast<APlayerCharacter>(OtherActor->GetInstigator());
        if (!PlayerChar && OtherActor->GetRootComponent())
        {
            for (AActor* Parent = OtherActor->GetAttachParentActor(); Parent; Parent = Parent->GetAttachParentActor())
            {
                PlayerChar = Cast<APlayerCharacter>(Parent);
                if (PlayerChar) break;
                // 부모 체인에서 UpperBodyPawn도 체크
                if (AUpperBodyPawn* ParentUpper = Cast<AUpperBodyPawn>(Parent))
                {
                    PlayerChar = ParentUpper->ParentBodyCharacter;
                    if (PlayerChar) break;
                }
            }
        }
        if (PlayerChar)
        {
            MyPS = PlayerChar->GetPlayerState<ABRPlayerState>();
        }
    }

    if (!PlayerChar || !MyPS)
    {
        return;
    }

    // 관전이거나 팀 없음(TeamID 0)이면 파트너 없음
    if (MyPS->bIsSpectatorSlot || MyPS->TeamNumber <= 0)
    {
        return;
    }

    ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
    if (!GS)
    {
        return;
    }

    // 파트너 찾기: 같은 TeamID(TeamNumber) + 반대 역할(하체↔상체). PlayerIndex(슬롯) 기준으로 같은 팀의 다른 1P/2P
    ABRPlayerState* PartnerPS = nullptr;
    int32 MyIndex = INDEX_NONE;
    int32 PartnerIndex = INDEX_NONE;
    for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
    {
        ABRPlayerState* Other = Cast<ABRPlayerState>(GS->PlayerArray[i]);
        if (!Other || Other == MyPS) continue;
        if (Other->TeamNumber != MyPS->TeamNumber) continue;
        if (Other->bIsSpectatorSlot) continue;
        // 같은 팀에서 하체/상체가 반대인 유일한 한 명 = 파트너
        if (Other->bIsLowerBody != MyPS->bIsLowerBody)
        {
            PartnerPS = Other;
            MyIndex = GS->PlayerArray.Find(MyPS);
            PartnerIndex = i;
            break;
        }
    }

    if (!PartnerPS || MyIndex == INDEX_NONE || PartnerIndex == INDEX_NONE)
    {
        return;
    }

    // [Step 1] 논리적 데이터 변경 (상체 <-> 하체). SetPlayerRole 시 상호 인덱스 전달해 ConnectedPlayerIndex 동기화
    bool MyNewRole = !MyPS->bIsLowerBody;
    bool PartnerNewRole = !PartnerPS->bIsLowerBody;

    MyPS->SetPlayerRole(MyNewRole, PartnerIndex);
    PartnerPS->SetPlayerRole(PartnerNewRole, MyIndex);

    // [Step 2] 실질적인 컨트롤러 스왑 및 부착 재설정 실행
    // 이 함수는 PlayerState에 새로 구현합니다.
    MyPS->SwapControlWithPartner();

    Destroy();
}