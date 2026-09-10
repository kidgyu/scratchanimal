// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SAActorManager.generated.h"

class ASAAnimalActor;
class ASAFoodActor;
class ASAFoodEffectActor;
class ASATrapEffectActor;
class ASATileActor;
class ASAMonsterActor;

/**
 * 게임 플레이 중 필요한 액터(동물, 음식, 이펙트 등)의 생성과 정리를 총괄하는 WorldSubsystem.
 * ASAGameState가 Ingame(Game) 상태로 진입할 때 이 매니저를 통해 SAAnimalActor/SAFoodActor를 생성하고,
 * 목표 타일 도착 시 SAFoodEffectActor를 생성하며, Trap 타일이 켜지고 꺼질 때마다 SATrapEffectActor를 풀링해서 재사용한다.
 */
UCLASS()
class SCRATCHANIMAL_API USAActorManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	USAActorManager();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 어디서든 쉽게 접근할 수 있는 정적 헬퍼 함수 */
	UFUNCTION(BlueprintPure, Category = "Actor Manager", meta = (WorldContext = "WorldContextObject"))
	static USAActorManager* Get(const UObject* WorldContextObject);

	// -------------------------------------------------------------
	// 설정 옵션
	// -------------------------------------------------------------

	/** 스폰할 동물 액터 클래스 (기본값: ASAAnimalActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Manager|Settings")
	TSubclassOf<ASAAnimalActor> AnimalActorClass;

	/** 동물 액터를 스폰할 임시 위치 (SAAnimalActor가 스스로 시작 타일 위치로 이동하므로 크게 중요하지 않음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Manager|Settings")
	FVector AnimalSpawnLocation;

	/** 스폰할 음식 액터 클래스 (기본값: ASAFoodActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Manager|Settings")
	TSubclassOf<ASAFoodActor> FoodActorClass;

	/** 음식 액터를 스폰할 임시 위치 (SAFoodActor가 스스로 목표 타일 위치로 이동하므로 크게 중요하지 않음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Manager|Settings")
	FVector FoodSpawnLocation;

	/** 스폰할 음식 이펙트 액터 클래스 (기본값: ASAFoodEffectActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Manager|Settings")
	TSubclassOf<ASAFoodEffectActor> FoodEffectActorClass;

	/** 풀링해서 재사용할 트랩 이펙트 액터 클래스 (기본값: ASATrapEffectActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Manager|Settings")
	TSubclassOf<ASATrapEffectActor> TrapEffectActorClass;

	/** 트랩 타일 중심 기준으로 이펙트를 얼마나 위(Z)에 표시할지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Manager|Settings")
	float TrapEffectZOffset;

	/** 스폰할 몬스터 액터 클래스 (기본값: ASAMonsterActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Manager|Settings")
	TSubclassOf<ASAMonsterActor> MonsterActorClass;

	/** 몬스터 액터를 스폰할 임시 위치 (ASAMonsterActor가 스스로 스폰 타일 위치로 이동하므로 크게 중요하지 않음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Manager|Settings")
	FVector MonsterSpawnLocation;

	// -------------------------------------------------------------
	// 주요 기능 인터페이스
	// -------------------------------------------------------------

	/** SAAnimalActor를 생성한다. 이미 생성되어 있으면 새로 만들지 않고 기존 인스턴스를 그대로 반환 */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	ASAAnimalActor* SpawnAnimalActor();

	/** 현재 생성되어 있는 동물 액터 반환 (없으면 nullptr) */
	UFUNCTION(BlueprintPure, Category = "Actor Manager")
	ASAAnimalActor* GetAnimalActor() const { return SpawnedAnimalActor; }

	/** SAFoodActor를 생성한다. 이미 생성되어 있으면 새로 만들지 않고 기존 인스턴스를 그대로 반환 */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	ASAFoodActor* SpawnFoodActor();

	/** 현재 생성되어 있는 음식 액터 반환 (없으면 nullptr) */
	UFUNCTION(BlueprintPure, Category = "Actor Manager")
	ASAFoodActor* GetFoodActor() const { return SpawnedFoodActor; }

	/** 음식 액터만 제거 (동물 액터는 그대로 유지) - 목표 타일 도착 시 호출 */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	void ClearFoodActor();

	/**
	 * 현재 레벨에 필요한 몬스터 마리 수(InCount)에 맞춰 몬스터 액터를 스폰/정리한다.
	 * 기존에 스폰되어 있던 몬스터는 그대로 유지하고(레벨이 바뀌면 각자 OnLevelLoaded를 받아 스스로 재배치됨),
	 * 부족하면 새로 스폰하고 남으면 뒤에서부터 제거해 목표 마리 수를 맞춘다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	void SpawnMonsterActors(int32 InCount);

	/** 현재 스폰되어 있는 몬스터 액터 목록 반환 */
	UFUNCTION(BlueprintPure, Category = "Actor Manager")
	TArray<ASAMonsterActor*> GetMonsterActors() const;

	/**
	 * 지정된 위치에 SAFoodEffectActor를 생성한다. 1초 뒤 스스로 파괴되는 1회성 이펙트라
	 * 동물/음식 액터와 달리 캐싱하지 않고 호출할 때마다 새로 생성된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	ASAFoodEffectActor* SpawnFoodEffectActor(const FVector& InLocation);

	/**
	 * 지정된 트랩 타일 위에 트랩 이펙트를 켠다 (Trap 타일이 Trap 상태가 될 때 호출).
	 * 풀에 여유분이 있으면 그것을 재사용하고, 없으면 새로 하나 생성한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	void ShowTrapEffect(ASATileActor* TrapTile);

	/** 지정된 트랩 타일에 표시 중이던 트랩 이펙트를 끄고 풀에 반납한다 (Trap 타일이 Normal로 돌아갈 때 호출) */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	void HideTrapEffect(ASATileActor* TrapTile);

	/** 현재 활성화되어 있는 모든 트랩 이펙트를 끄고 전부 풀에 반납한다 (레벨이 클리어되어 트랩 타일들이 파괴되기 직전/직후에 호출) */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	void ReleaseAllTrapEffects();

	/**
	 * 풀에 대기 중이거나 현재 활성화된 트랩 이펙트 액터를 전부 완전히 파괴한다 (풀에 반납하지 않음).
	 * Trap 발동이나 10단계 클리어 같은 "레벨 이탈 연출"에서 트랩 이펙트를 완전히 없앨 때 호출한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	void DestroyTrapEffectPool();

	/** 이 매니저가 생성한 모든 액터(동물, 음식 등)를 제거하고 참조를 비움 */
	UFUNCTION(BlueprintCallable, Category = "Actor Manager")
	void ClearManagedActors();

private:
	/** 현재 생성되어 있는 동물 액터 */
	UPROPERTY(Transient)
	TObjectPtr<ASAAnimalActor> SpawnedAnimalActor;

	/** 현재 생성되어 있는 음식 액터 */
	UPROPERTY(Transient)
	TObjectPtr<ASAFoodActor> SpawnedFoodActor;

	/** 현재 생성되어 있는 몬스터 액터 목록 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASAMonsterActor>> SpawnedMonsterActors;

	/** SATileManager의 OnTilesCleared 이벤트를 받아 활성 트랩 이펙트를 전부 풀에 반납하는 핸들러 */
	UFUNCTION()
	void HandleTilesCleared();

	/** 풀에서 재사용 가능한 트랩 이펙트를 하나 꺼내거나(없으면) 새로 생성 */
	ASATrapEffectActor* AcquirePooledTrapEffectActor();

	/** 재사용 대기 중인(현재 꺼져 있는) 트랩 이펙트 풀 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASATrapEffectActor>> FreeTrapEffectPool;

	/** 트랩 타일 별로 현재 표시 중인 트랩 이펙트 매핑 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<ASATileActor>, TObjectPtr<ASATrapEffectActor>> ActiveTrapEffects;
};
