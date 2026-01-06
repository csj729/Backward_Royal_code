// InteractableInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

// 언리얼 리플렉션 시스템용 (수정 불필요)
UINTERFACE(MinimalAPI)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 상호작용(E키) 가능한 모든 액터가 구현해야 할 인터페이스입니다.
 * 무기, 아이템, 문, 차량 등 모든 상호작용 대상에 적용됩니다.
 */
class BACKWARD_ROYAL_API IInteractableInterface
{
	GENERATED_BODY()

public:
	// 1. 상호작용 실행
	// Character: 상호작용을 시도한 주체 (플레이어)
	virtual void Interact(class ABaseCharacter* Character) = 0;

	// 2. 상호작용 안내 문구 반환
	// UI에 표시할 텍스트 (예: "줍기: 롱소드", "문 열기")
	virtual FText GetInteractionPrompt() = 0;
};