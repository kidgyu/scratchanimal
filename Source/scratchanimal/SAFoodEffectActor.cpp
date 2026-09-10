// Fill out your copyright notice in the Description page of Project Settings.

#include "SAFoodEffectActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

ASAFoodEffectActor::ASAFoodEffectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	FoodEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FoodEffect"));
	RootComponent = FoodEffect;
	FoodEffect->bAutoActivate = true; // 생성되는 즉시 한 번 재생

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FoodEffectFinder(TEXT("/Game/Effect/Effect_Food.Effect_Food"));
	if (FoodEffectFinder.Succeeded())
	{
		FoodEffect->SetAsset(FoodEffectFinder.Object);
	}

	LifeTime = 1.0f;
}

void ASAFoodEffectActor::BeginPlay()
{
	Super::BeginPlay();

	// bAutoActivate로 이미 재생이 시작되지만, 재사용 없이 항상 처음부터 한 번만 재생되도록 명시적으로도 호출
	if (FoodEffect)
	{
		FoodEffect->Activate(true);
	}

	// LifeTime이 지나면 이펙트 재생 여부와 상관없이 액터 스스로 파괴
	GetWorldTimerManager().SetTimer(DestroyTimerHandle, this, &ASAFoodEffectActor::DestroySelf, LifeTime, false);
}

void ASAFoodEffectActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASAFoodEffectActor::DestroySelf()
{
	Destroy();
}
