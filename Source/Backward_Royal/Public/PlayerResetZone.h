#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
// 박스 컴포넌트를 사용하기 위한 헤더 추가
#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "PlayerResetZone.generated.h"

class UTexture2D;

UCLASS()
class BACKWARD_ROYAL_API APlayerResetZone : public AActor
{
	GENERATED_BODY()

public:
	// 생성자
	APlayerResetZone();

protected:
	// 게임 시작 시 호출
	virtual void BeginPlay() override;

public:
	// [핵심] 무언가가 이 박스에 겹쳤을 때 실행될 함수
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	// 충돌을 감지할 트리거 박스 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone Settings", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerBox;

	// (선택사항) 에디터에서 쉽게 볼 수 있게 해주는 빌보드 아이콘 (실제 게임에선 안 보임)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone Settings", meta = (AllowPrivateAccess = "true"))
	UBillboardComponent* SpriteIcon;
};