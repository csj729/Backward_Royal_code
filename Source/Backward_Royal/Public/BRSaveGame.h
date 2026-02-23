// BRSaveGame.h - 플레이어 이름·커스텀 등 로컬 보존용
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CustomizationInfo.h"
#include "BRSaveGame.generated.h"

UCLASS()
class BACKWARD_ROYAL_API UBRPlayerSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UBRPlayerSettingsSaveGame();

	UPROPERTY(VisibleAnywhere, Category = "Save")
	FString SavedPlayerName;

	UPROPERTY(VisibleAnywhere, Category = "Save")
	FBRCustomizationData SavedCustomization;

	UPROPERTY(VisibleAnywhere, Category = "Save")
	FString SavedUserUID;
};
