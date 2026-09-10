// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SAFoodEffectActor.generated.h"

class UNiagaraComponent;

/**
 * 목표 타일에서 SAFoodActor를 먹었을 때 보여주는 1회성 이펙트 액터.
 * 생성되면 Niagara 이펙트를 한 번만 재생하고, LifeTime(기본 1초) 후 스스로 파괴된다.
 */
UCLASS()
class SCRATCHANIMAL_API ASAFoodEffectActor : public AActor
{
	GENERATED_BODY()

public:
	ASAFoodEffectActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// 컴포넌트
	// -------------------------------------------------------------

	/** 재생할 나이아가라 이펙트 컴포넌트 (콜라이더 없이 루트 컴포넌트로 사용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> FoodEffect;

	// -------------------------------------------------------------
	// 설정
	// -------------------------------------------------------------

	/** 이펙트 재생 후 액터가 스스로 파괴되기까지의 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food Effect", meta = (ClampMin = "0.01"))
	float LifeTime;

private:
	/** LifeTime 타이머에 바인딩되는 자기 파괴 함수 (AActor::Destroy는 반환값이 있어 타이머에 직접 바인딩할 수 없음) */
	void DestroySelf();

	/** 자기 파괴 타이머 핸들 */
	FTimerHandle DestroyTimerHandle;
};
