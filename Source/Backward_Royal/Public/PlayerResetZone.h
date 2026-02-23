#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
// �ڽ� ������Ʈ�� ����ϱ� ���� ��� �߰�
#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "PlayerResetZone.generated.h"

class UTexture2D;

UCLASS()
class BACKWARD_ROYAL_API APlayerResetZone : public AActor
{
	GENERATED_BODY()

public:
	// ������
	APlayerResetZone();

protected:
	// ���� ���� ��
	virtual void BeginPlay() override;

public:
	// [�ٽ�] ���𰡰� �� �ڽ��� ������ �� ����� �Լ�
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	// �浹�� ������ ������ �ڽ� ������Ʈ
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone Settings", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerBox;

	// (���û���) �����Ϳ��� ���� �� ��� �ϱ� ���� ������ ������ (���� ���ӿ��� �� ����)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone Settings", meta = (AllowPrivateAccess = "true"))
	UBillboardComponent* SpriteIcon;
};