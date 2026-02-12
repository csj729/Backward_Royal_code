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

// SwitchOrb.cpp

void ASwitchOrb::OnOrbOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 1. 권한 확인
    if (!HasAuthority())
    {
        return;
    }

    // [수정 1] 이미 누군가에 의해 작동 중이라면 즉시 리턴 (중복 스왑 방지)
    if (bIsTriggered)
    {
        return;
    }

    if (!OtherActor) return;

    // 플레이어 찾기 로직 (기존 코드 유지)
    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(OtherActor);
    ABRPlayerState* MyPS = nullptr;

    // 1) 직접 PlayerCharacter인 경우
    if (PlayerChar)
    {
        MyPS = PlayerChar->GetPlayerState<ABRPlayerState>();
    }
    // 2) UpperBodyPawn인 경우
    else if (AUpperBodyPawn* UpperPawn = Cast<AUpperBodyPawn>(OtherActor))
    {
        if (AController* Controller = UpperPawn->GetController())
        {
            MyPS = Controller->GetPlayerState<ABRPlayerState>();
            PlayerChar = UpperPawn->ParentBodyCharacter;
        }
        if (!MyPS || !PlayerChar) return;
    }
    // 3) 무기/부착물 등 - 부모 체인 탐색
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

    if (!PlayerChar || !MyPS) return;

    // 관전이거나 팀 없음 처리
    if (MyPS->bIsSpectatorSlot || MyPS->TeamNumber <= 0) return;

    ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
    if (!GS) return;

    // 파트너 찾기 (기존 코드 유지)
    ABRPlayerState* PartnerPS = nullptr;
    int32 MyIndex = INDEX_NONE;
    int32 PartnerIndex = INDEX_NONE;

    for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
    {
        ABRPlayerState* Other = Cast<ABRPlayerState>(GS->PlayerArray[i]);
        if (!Other || Other == MyPS) continue;
        if (Other->TeamNumber != MyPS->TeamNumber) continue;
        if (Other->bIsSpectatorSlot) continue;

        if (Other->bIsLowerBody != MyPS->bIsLowerBody)
        {
            PartnerPS = Other;
            MyIndex = GS->PlayerArray.Find(MyPS);
            PartnerIndex = i;
            break;
        }
    }

    if (!PartnerPS || MyIndex == INDEX_NONE || PartnerIndex == INDEX_NONE) return;

    // [수정 2] 모든 검증이 끝났으므로 트리거 발동 확정 (이후 중복 호출 차단)
    bIsTriggered = true;

    // [Step 1] 논리적 데이터 변경
    bool MyNewRole = !MyPS->bIsLowerBody;
    bool PartnerNewRole = !PartnerPS->bIsLowerBody;

    MyPS->SetPlayerRole(MyNewRole, PartnerIndex);
    PartnerPS->SetPlayerRole(PartnerNewRole, MyIndex);

    // [Step 2] 실질적인 컨트롤러 스왑 및 부착 재설정 실행
    MyPS->SwapControlWithPartner();

    Destroy();
}