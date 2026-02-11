#include "SimpleMovingPlatform.h"

// 생성자: 기본값 설정
ASimpleMovingPlatform::ASimpleMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true; // 움직여야 하니까 Tick 켜기

	// 메시 컴포넌트 만들기
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	// 기본 설정값 (에디터에서 수정 가능)
	MoveOffset = FVector(0.0f, 0.0f, 300.0f); // 위로 300만큼
	MoveSpeed = 1.0f; // 속도 1.0
}

// 게임 시작 시
void ASimpleMovingPlatform::BeginPlay()
{
	Super::BeginPlay();

	// 배치된 현재 위치를 '시작점'으로 기억
	StartLocation = GetActorLocation();
}

// 매 프레임 (움직임 로직)
void ASimpleMovingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 게임 실행 후 흐른 시간 가져오기
	float RunningTime = GetGameTimeSinceCreation();

	// 공식: 시작위치 + (이동거리 * 사인파(시간 * 속도))
	// FMath::Sin은 -1 ~ 1 사이를 부드럽게 왕복하는 값을 줍니다.
	FVector NewLocation = StartLocation + (MoveOffset * FMath::Sin(RunningTime * MoveSpeed));

	// 위치 적용
	SetActorLocation(NewLocation);
}