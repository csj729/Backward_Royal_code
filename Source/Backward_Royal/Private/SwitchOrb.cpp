// SwitchOrb.cpp
#include "SwitchOrb.h"
#include "Components/SphereComponent.h"
#include "PlayerCharacter.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BRPlayerState.h"
#include "BRGameState.h"

DEFINE_LOG_CATEGORY(LogSwitchOrb);

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
    if (!HasAuthority() || !OtherActor) return;

    APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(OtherActor);
    if (!PlayerChar) return;

    ABRPlayerState* MyPS = PlayerChar->GetPlayerState<ABRPlayerState>();
    if (!MyPS || MyPS->ConnectedPlayerIndex == -1) return;

    // 파트너 찾기
    ABRGameState* GS = GetWorld()->GetGameState<ABRGameState>();
    if (!GS || !GS->PlayerArray.IsValidIndex(MyPS->ConnectedPlayerIndex)) return;

    ABRPlayerState* PartnerPS = Cast<ABRPlayerState>(GS->PlayerArray[MyPS->ConnectedPlayerIndex]);
    if (!PartnerPS) return;

    // [Step 1] 논리적 데이터 변경 (상체 <-> 하체)
    bool MyNewRole = !MyPS->bIsLowerBody;
    bool PartnerNewRole = !PartnerPS->bIsLowerBody;

    MyPS->SetPlayerRole(MyNewRole, MyPS->ConnectedPlayerIndex);
    PartnerPS->SetPlayerRole(PartnerNewRole, PartnerPS->ConnectedPlayerIndex);

    // [Step 2] 실질적인 컨트롤러 스왑 및 부착 재설정 실행
    // 이 함수는 PlayerState에 새로 구현합니다.
    MyPS->SwapControlWithPartner();

    ORB_LOG(Log, TEXT("Orb 조작: %s와 %s의 역할 및 제어권 스왑 실행"), *MyPS->GetPlayerName(), *PartnerPS->GetPlayerName());

    Destroy();
}