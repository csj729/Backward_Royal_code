#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimpleMovingPlatform.generated.h"

UCLASS()
class BACKWARD_ROYAL_API ASimpleMovingPlatform : public AActor
{
	GENERATED_BODY()

public:
	// 생성자
	ASimpleMovingPlatform();

protected:
	// 게임 시작할 때 한 번 실행
	virtual void BeginPlay() override;

public:
	// 매 프레임 실행
	virtual void Tick(float DeltaTime) override;

private:
	// 1. 눈에 보이는 메시 (디테일 패널에서 메시 모양 설정 가능)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Platform Settings", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* MeshComp;

	// 2. 시작 위치 저장용 변수
	FVector StartLocation;

	// 3. [설정] 얼마나 움직일 것인가? (X, Y, Z)
	UPROPERTY(EditAnywhere, Category = "Platform Settings")
	FVector MoveOffset;

	// 4. [설정] 움직이는 속도
	UPROPERTY(EditAnywhere, Category = "Platform Settings")
	float MoveSpeed;
};