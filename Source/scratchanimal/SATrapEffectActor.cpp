// Fill out your copyright notice in the Description page of Project Settings.

#include "SATrapEffectActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ASATrapEffectActor::ASATrapEffectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TrapEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrapEffect"));
	RootComponent = TrapEffect;
	TrapEffect->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrapEffect->SetCollisionProfileName(TEXT("NoCollision"));
	TrapEffect->SetGenerateOverlapEvents(false);
	TrapEffect->bAutoActivate = false; // 풀에서 대기하는 동안에는 재생하지 않고, ActivateAt 호출 시에만 재생

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TrapEffectFinder(TEXT("/Game/Effect/Effect_Trap.Effect_Trap"));
	if (TrapEffectFinder.Succeeded())
	{
		TrapEffect->SetAsset(TrapEffectFinder.Object);
	}

	EffectScale = 0.3f; // 원본 이펙트 반경이 너무 커서 절반으로 축소
	TrapEffect->SetRelativeScale3D(FVector(EffectScale));

	// 풀에 대기 중인 초기 상태에서는 숨겨둔다
	SetActorHiddenInGame(true);
}

void ASATrapEffectActor::BeginPlay()
{
	Super::BeginPlay();
}

void ASATrapEffectActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASATrapEffectActor::ActivateAt(const FVector& InLocation)
{
	SetActorLocation(InLocation);
	SetActorHiddenInGame(false);

	if (TrapEffect)
	{
		TrapEffect->Activate(true);
	}
}

void ASATrapEffectActor::DeactivateEffect()
{
	if (TrapEffect)
	{
		TrapEffect->Deactivate();
	}

	SetActorHiddenInGame(true);
}
