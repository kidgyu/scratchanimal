// SASoundSettings.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SATypes.h"
#include "SASoundSettings.generated.h"

UCLASS(Config=Game, defaultconfig, meta=(DisplayName="SA Sound Setting"))
class SCRATCHANIMAL_API USASoundSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio|BGM")
	TMap<ESABGMType, TSoftObjectPtr<USoundBase>> BGMList;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Audio|SFX")
	TMap<ESASFXType, TSoftObjectPtr<USoundBase>> SFXList;
};
