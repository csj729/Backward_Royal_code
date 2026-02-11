#include "PlayerResetZone.h"
// --- 필수 헤더 파일 추가 ---
#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "GameFramework/Pawn.h"          // 플레이어(폰) 인식용
#include "Kismet/GameplayStatics.h"      // 플레이어 스타트 찾기용
#include "GameFramework/PlayerStart.h"   // 플레이어 스타트 클래스
// ---------------------------

// 생성자
APlayerResetZone::APlayerResetZone()
{
	// 이 액터는 움직이지 않으므로 Tick이 필요 없습니다. 최적화를 위해 끕니다.
	PrimaryActorTick.bCanEverTick = false;

	// 1. 트리거 박스 생성 및 설정
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox; // 이 박스를 루트로 설정

	// 박스 크기 기본값 설정 (에디터에서 스케일로 조절 가능)
	TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));

	// [중요] 콜리전 설정: "Trigger" 프리셋 사용
	// Trigger 프리셋은 기본적으로 겹침(Overlap)만 감지하고 물리적으로 막지 않습니다.
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));


	// 2. (선택사항) 에디터 아이콘 설정
#if WITH_EDITORONLY_DATA
	SpriteIcon = CreateDefaultSubobject<UBillboardComponent>(TEXT("SpriteIcon"));
	SpriteIcon->SetupAttachment(RootComponent);
	// 에디터에서 쉽게 선택하기 위한 아이콘 로드 (엔진 기본 아이콘 사용)
	static ConstructorHelpers::FObjectFinder<UTexture2D> IconTexture(TEXT("/Engine/EditorResources/S_Trigger"));
	if (IconTexture.Succeeded())
	{
		SpriteIcon->SetSprite(IconTexture.Object);
		SpriteIcon->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
	}
#endif
}

void APlayerResetZone::BeginPlay()
{
	Super::BeginPlay();

	// [핵심] 이벤트 연결
	// "TriggerBox에 무언가 겹치면 -> OnOverlapBegin 함수를 실행해라"
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &APlayerResetZone::OnOverlapBegin);
}

// 실제 리셋 로직 구현
void APlayerResetZone::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 1. 닿은 대상이 유효한지, 그리고 자기 자신은 아닌지 체크
	if (!OtherActor || OtherActor == this) return;

	// 2. 닿은 대상이 '플레이어(Pawn)'인지 확인
	// (몬스터나 다른 물체는 닿아도 리셋되면 안 되니까요)
	APawn* PlayerPawn = Cast<APawn>(OtherActor);

	if (PlayerPawn)
	{
		// 3. 월드에 있는 모든 'PlayerStart' 액터를 찾습니다.
		TArray<AActor*> FoundPlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), FoundPlayerStarts);

		// PlayerStart가 하나라도 있다면
		if (FoundPlayerStarts.Num() > 0)
		{
			// 첫 번째로 발견된 PlayerStart의 위치를 가져옵니다.
			FVector RestartLocation = FoundPlayerStarts[0]->GetActorLocation();

			// 4. 플레이어를 해당 위치로 순간이동시킵니다.
			PlayerPawn->SetActorLocation(RestartLocation);

			// 로그 출력 (테스트용)
			UE_LOG(LogTemp, Warning, TEXT("[%s]가 위험 구역에 진입하여 리셋되었습니다!"), *PlayerPawn->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("맵에 PlayerStart가 없습니다! 리셋할 수 없습니다."));
		}
	}
}