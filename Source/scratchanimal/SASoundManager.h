// SASoundManager.h

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SASoundSettings.h"
#include "SASoundManager.generated.h"

enum class ESABGMType : uint8;
enum class ESASFXType : uint8;

class USoundBase;
class UAudioComponent;

UCLASS()
class SCRATCHANIMAL_API USASoundManager : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** 어디서든 쉽게 접근할 수 있는 정적 헬퍼 함수 */
	UFUNCTION(BlueprintPure, Category = "Sound Manager", meta = (WorldContext = "WorldContextObject"))
	static USASoundManager* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SoundManager|BGM")
	void PlayBGM(ESABGMType BGMType, float FadeInTime = 1.f);

	UFUNCTION(BlueprintCallable, Category = "SoundManager|BGM")
	void StopBGM(float FadeOutTime = 0.f);

	UFUNCTION(BlueprintPure, Category = "SoundManager|BGM")
	ESABGMType GetCurrentBGMType() const { return CurrentBGMType; }

	UFUNCTION(BlueprintCallable, Category = "SoundManager|SFX")
	void PlaySound2D(ESASFXType SFXType, float VolumeMultiplier = 1.f);

	UFUNCTION(BlueprintCallable, Category = "SoundManager|SFX")
	bool IsPlayingSound2D(ESASFXType SFXType);

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> CurrentBGMComponent;

	UPROPERTY()
	ESABGMType CurrentBGMType = ESABGMType::None;

	UPROPERTY()
	TMap<ESASFXType, TObjectPtr<USoundBase>> LoadedSFXMap;

	UPROPERTY()
	TMap<ESASFXType, TWeakObjectPtr<UAudioComponent>> ActiveSFXCompoents;
};
