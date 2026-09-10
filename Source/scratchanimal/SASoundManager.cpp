// SASoundManager.cpp

#include "SASoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"

USASoundManager* USASoundManager::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<USASoundManager>() : nullptr;
}

void USASoundManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentBGMComponent = nullptr;
	CurrentBGMType = ESABGMType::None;
}

void USASoundManager::Deinitialize()
{
	StopBGM(0.f);

	LoadedSFXMap.Empty();
	ActiveSFXCompoents.Empty();

	Super::Deinitialize();
}

void USASoundManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const USASoundSettings* SoundSettings = GetDefault<USASoundSettings>();
	if (!SoundSettings)
		return;

	for (const auto& Pair : SoundSettings->SFXList)
	{
		ESASFXType Type = Pair.Key;
		const TSoftObjectPtr<USoundBase>& SoftPtr = Pair.Value;
		if (USoundBase* LoadedSFX = SoftPtr.LoadSynchronous())
		{
			LoadedSFXMap.Add(Type, LoadedSFX);
		}
	}
}

void USASoundManager::PlayBGM(ESABGMType BGMType, float FadeInTime)
{
	if (BGMType == ESABGMType::None)
		return;

	// 이미 동일한 BGM이 재생 중이라면 중복 재생 방지
	if (CurrentBGMType == BGMType && CurrentBGMComponent && CurrentBGMComponent->IsPlaying())
	{
		return;
	}

	// 기존에 다른 BGM이 재생 중이었다면 정지
	if (CurrentBGMComponent && CurrentBGMComponent->IsPlaying())
	{
		StopBGM(FadeInTime > 0.f ? FadeInTime * 0.5f : 0.f);
	}

	const USASoundSettings* SoundSettings = GetDefault<USASoundSettings>();
	if (const TSoftObjectPtr<USoundBase>* FoundSoundPtr = SoundSettings->BGMList.Find(BGMType))
	{
		if (USoundBase* LoadedBGM = FoundSoundPtr->LoadSynchronous())
		{
			if (UWorld* World = GetWorld())
			{
				CurrentBGMComponent = UGameplayStatics::CreateSound2D(
					World,
					LoadedBGM,
					1.f,
					1.f,
					0.f,
					nullptr,
					true,
					false
				);

				if (CurrentBGMComponent)
				{
					CurrentBGMComponent->FadeIn(FadeInTime);
					CurrentBGMType = BGMType;
				}
			}
		}
	}
}

void USASoundManager::StopBGM(float FadeOutTime)
{
	if (CurrentBGMComponent && CurrentBGMComponent->IsPlaying())
	{
		if (FadeOutTime > 0.f)
		{
			CurrentBGMComponent->FadeOut(FadeOutTime, 0.f);
		}
		else
		{
			CurrentBGMComponent->Stop();
		}
	}

	CurrentBGMComponent = nullptr;
	CurrentBGMType = ESABGMType::None;
}

void USASoundManager::PlaySound2D(ESASFXType SFXType, float VolumeMultiplier)
{
	if (SFXType == ESASFXType::None)
		return;

	if (TObjectPtr<USoundBase>* FoundSound = LoadedSFXMap.Find(SFXType))
	{
		if (*FoundSound)
		{
			UAudioComponent* AudioComp = UGameplayStatics::SpawnSound2D(GetWorld(), *FoundSound, VolumeMultiplier);
			if (AudioComp)
			{
				if (ActiveSFXCompoents.Contains(SFXType))
				{
					ActiveSFXCompoents.Remove(SFXType);
				}

				ActiveSFXCompoents.Add(SFXType, AudioComp);
			}
		}
	}
}

bool USASoundManager::IsPlayingSound2D(ESASFXType SFXType)
{
	if (!LoadedSFXMap.Contains(SFXType))
		return false;

	if (TWeakObjectPtr<UAudioComponent>* ActiveCompPtr = ActiveSFXCompoents.Find(SFXType))
	{
		if (ActiveCompPtr->IsValid())
		{
			return (*ActiveCompPtr)->IsPlaying();
		}
		else
		{
			if (ActiveSFXCompoents.Contains(SFXType))
			{
				ActiveSFXCompoents.Remove(SFXType);
			}
		}
	}

	return false;
}