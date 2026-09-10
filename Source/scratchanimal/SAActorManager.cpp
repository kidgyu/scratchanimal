// Fill out your copyright notice in the Description page of Project Settings.

#include "SAActorManager.h"
#include "SAAnimalActor.h"
#include "SAFoodActor.h"
#include "SAFoodEffectActor.h"
#include "SATrapEffectActor.h"
#include "SAMonsterActor.h"
#include "SATileActor.h"
#include "SATileManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

USAActorManager::USAActorManager()
{
	AnimalActorClass = ASAAnimalActor::StaticClass();
	AnimalSpawnLocation = FVector::ZeroVector;

	FoodActorClass = ASAFoodActor::StaticClass();
	FoodSpawnLocation = FVector::ZeroVector;

	FoodEffectActorClass = ASAFoodEffectActor::StaticClass();

	TrapEffectActorClass = ASATrapEffectActor::StaticClass();
	TrapEffectZOffset = 15.0f;

	MonsterActorClass = ASAMonsterActor::StaticClass();
	MonsterSpawnLocation = FVector::ZeroVector;
}

void USAActorManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 레벨이 클리어되어 트랩 타일들이 파괴될 때(OnTilesCleared) 활성 트랩 이펙트를 전부 풀에 반납
	if (USATileManager* TileManager = USATileManager::Get(this))
	{
		TileManager->OnTilesCleared.AddDynamic(this, &USAActorManager::HandleTilesCleared);
	}
}

USAActorManager* USAActorManager::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<USAActorManager>() : nullptr;
}

ASAAnimalActor* USAActorManager::SpawnAnimalActor()
{
	if (IsValid(SpawnedAnimalActor))
	{
		return SpawnedAnimalActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ClassToSpawn = AnimalActorClass ? AnimalActorClass.Get() : ASAAnimalActor::StaticClass();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SpawnedAnimalActor = World->SpawnActor<ASAAnimalActor>(ClassToSpawn, AnimalSpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (!SpawnedAnimalActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[USAActorManager] SAAnimalActor 스폰 실패!"));
	}

	return SpawnedAnimalActor;
}

ASAFoodActor* USAActorManager::SpawnFoodActor()
{
	if (IsValid(SpawnedFoodActor))
	{
		return SpawnedFoodActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ClassToSpawn = FoodActorClass ? FoodActorClass.Get() : ASAFoodActor::StaticClass();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SpawnedFoodActor = World->SpawnActor<ASAFoodActor>(ClassToSpawn, FoodSpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (!SpawnedFoodActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[USAActorManager] SAFoodActor 스폰 실패!"));
	}

	return SpawnedFoodActor;
}

ASAFoodEffectActor* USAActorManager::SpawnFoodEffectActor(const FVector& InLocation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ClassToSpawn = FoodEffectActorClass ? FoodEffectActorClass.Get() : ASAFoodEffectActor::StaticClass();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASAFoodEffectActor* NewEffect = World->SpawnActor<ASAFoodEffectActor>(ClassToSpawn, InLocation, FRotator::ZeroRotator, SpawnParams);
	if (!NewEffect)
	{
		UE_LOG(LogTemp, Error, TEXT("[USAActorManager] SAFoodEffectActor 스폰 실패!"));
	}

	return NewEffect;
}

void USAActorManager::ClearFoodActor()
{
	if (IsValid(SpawnedFoodActor))
	{
		SpawnedFoodActor->Destroy();
	}
	SpawnedFoodActor = nullptr;
}

void USAActorManager::SpawnMonsterActors(int32 InCount)
{
	const int32 TargetCount = FMath::Max(0, InCount);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 목표 마리 수보다 많으면 뒤에서부터 제거
	while (SpawnedMonsterActors.Num() > TargetCount)
	{
		const int32 LastIndex = SpawnedMonsterActors.Num() - 1;
		if (ASAMonsterActor* MonsterToRemove = SpawnedMonsterActors[LastIndex])
		{
			if (IsValid(MonsterToRemove))
			{
				MonsterToRemove->Destroy();
			}
		}
		SpawnedMonsterActors.RemoveAt(LastIndex);
	}

	// 목표 마리 수보다 적으면 부족한 만큼 새로 스폰
	UClass* ClassToSpawn = MonsterActorClass ? MonsterActorClass.Get() : ASAMonsterActor::StaticClass();
	while (SpawnedMonsterActors.Num() < TargetCount)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASAMonsterActor* NewMonster = World->SpawnActor<ASAMonsterActor>(ClassToSpawn, MonsterSpawnLocation, FRotator::ZeroRotator, SpawnParams);
		if (!NewMonster)
		{
			UE_LOG(LogTemp, Error, TEXT("[USAActorManager] SAMonsterActor 스폰 실패!"));
			break;
		}

		SpawnedMonsterActors.Add(NewMonster);
	}
}

TArray<ASAMonsterActor*> USAActorManager::GetMonsterActors() const
{
	TArray<ASAMonsterActor*> Result;
	Result.Reserve(SpawnedMonsterActors.Num());
	for (const TObjectPtr<ASAMonsterActor>& Monster : SpawnedMonsterActors)
	{
		if (IsValid(Monster))
		{
			Result.Add(Monster);
		}
	}
	return Result;
}

void USAActorManager::ShowTrapEffect(ASATileActor* TrapTile)
{
	if (!TrapTile || ActiveTrapEffects.Contains(TrapTile))
	{
		return;
	}

	ASATrapEffectActor* Effect = AcquirePooledTrapEffectActor();
	if (!Effect)
	{
		return;
	}

	Effect->ActivateAt(TrapTile->GetActorLocation() + FVector(0.0f, 0.0f, TrapEffectZOffset));
	ActiveTrapEffects.Add(TrapTile, Effect);
}

void USAActorManager::HideTrapEffect(ASATileActor* TrapTile)
{
	if (!TrapTile)
	{
		return;
	}

	TObjectPtr<ASATrapEffectActor> FoundEffect;
	if (ActiveTrapEffects.RemoveAndCopyValue(TrapTile, FoundEffect) && IsValid(FoundEffect))
	{
		FoundEffect->DeactivateEffect();
		FreeTrapEffectPool.Add(FoundEffect);
	}
}

void USAActorManager::ReleaseAllTrapEffects()
{
	for (const auto& Pair : ActiveTrapEffects)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->DeactivateEffect();
			FreeTrapEffectPool.Add(Pair.Value);
		}
	}
	ActiveTrapEffects.Empty();
}

void USAActorManager::DestroyTrapEffectPool()
{
	for (auto& Pair : ActiveTrapEffects)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}
	ActiveTrapEffects.Empty();

	for (auto& Pooled : FreeTrapEffectPool)
	{
		if (IsValid(Pooled))
		{
			Pooled->Destroy();
		}
	}
	FreeTrapEffectPool.Empty();
}

void USAActorManager::HandleTilesCleared()
{
	ReleaseAllTrapEffects();
}

ASATrapEffectActor* USAActorManager::AcquirePooledTrapEffectActor()
{
	// 풀에 재사용 가능한 이펙트가 있으면 꺼내 쓰고, 없으면 새로 생성
	while (!FreeTrapEffectPool.IsEmpty())
	{
		ASATrapEffectActor* Pooled = FreeTrapEffectPool.Pop();
		if (IsValid(Pooled))
		{
			return Pooled;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ClassToSpawn = TrapEffectActorClass ? TrapEffectActorClass.Get() : ASATrapEffectActor::StaticClass();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASATrapEffectActor* NewEffect = World->SpawnActor<ASATrapEffectActor>(ClassToSpawn, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!NewEffect)
	{
		UE_LOG(LogTemp, Error, TEXT("[USAActorManager] SATrapEffectActor 스폰 실패!"));
	}

	return NewEffect;
}

void USAActorManager::ClearManagedActors()
{
	if (IsValid(SpawnedAnimalActor))
	{
		SpawnedAnimalActor->Destroy();
	}
	SpawnedAnimalActor = nullptr;

	ClearFoodActor();

	for (const TObjectPtr<ASAMonsterActor>& Monster : SpawnedMonsterActors)
	{
		if (IsValid(Monster))
		{
			Monster->Destroy();
		}
	}
	SpawnedMonsterActors.Empty();
}
