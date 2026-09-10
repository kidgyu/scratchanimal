// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SATrapEffectActor.generated.h"

class UNiagaraComponent;

/**
 * Trap 타일이 Trap 상태가 될 때 그 위에 표시되는 나이아가라 이펙트 액터.
 * Trap은 Normal ↔ Trap을 계속 반복하므로, 매번 새로 스폰/파괴하지 않고
 * SAActorManager가 풀링해서 재사용한다 (ActivateAt으로 켜고, DeactivateEffect로 끄고 풀에 반납).
 */
UCLASS()
class SCRATCHANIMAL_API ASATrapEffectActor : public AActor
{
	GENERATED_BODY()

public:
	ASATrapEffectActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// 컴포넌트
	// -------------------------------------------------------------

	/** 재생할 나이아가라 이펙트 컴포넌트 (콜라이더 없이 루트 컴포넌트로 사용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> TrapEffect;

	/** 이펙트 전체 크기 배율 (반경 포함, 원본 대비 축소/확대) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trap Effect")
	float EffectScale;

	// -------------------------------------------------------------
	// 풀링 인터페이스 (SAActorManager가 호출)
	// -------------------------------------------------------------

	/** 지정된 위치로 옮기고 이펙트를 켜서 보여준다 (풀에서 꺼내 쓸 때 호출) */
	UFUNCTION(BlueprintCallable, Category = "Trap Effect")
	void ActivateAt(const FVector& InLocation);

	/** 이펙트를 끄고 숨긴다 (풀에 반납될 때 호출 - 파괴하지 않고 대기 상태로 전환) */
	UFUNCTION(BlueprintCallable, Category = "Trap Effect")
	void DeactivateEffect();
};
