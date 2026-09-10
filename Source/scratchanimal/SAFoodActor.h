// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SAFoodActor.generated.h"

class UStaticMeshComponent;
class ASATileActor;
class USATileManager;

/**
 * 목표(Goal) 타일 위에 표시되는 음식 액터. 콜라이더 없이, 레벨이 시작되면 목표 타일 위치로 이동하고
 * 제자리에서 천천히 회전하며 위아래로 부드럽게 움직인다.
 */
UCLASS()
class SCRATCHANIMAL_API ASAFoodActor : public AActor
{
	GENERATED_BODY()

public:
	ASAFoodActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// 컴포넌트
	// -------------------------------------------------------------

	/** 음식 외형 스태틱 메쉬 (콜라이더 없이 루트 컴포넌트로 사용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FoodMesh;

	// -------------------------------------------------------------
	// 연출 설정
	// -------------------------------------------------------------

	/** 초당 제자리 회전 속도 (Yaw, degree/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food|Animation")
	float RotationSpeed;

	/** 위아래로 움직이는 전체 폭 (최고점과 최저점의 좌표 차이) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food|Animation")
	float BobDistance;

	/** 위아래 움직임의 속도 (클수록 빠르게 왕복) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food|Animation")
	float BobSpeed;

	/** 목표 타일 중심 기준으로 얼마나 위(Z)에 올려놓을지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food|Animation")
	float TileSurfaceZOffset;

private:
	/** SATileManager의 이벤트에 바인딩해서 레벨이 새로 로드될 때 목표 타일 위치로 즉시 이동하도록 한다 */
	void BindToTileManager();

	UFUNCTION()
	void HandleLevelLoaded(int32 LevelNumber);

	/** 지정된 타일의 위치(+ Z 오프셋)를 회전/상하 움직임의 기준 위치로 삼아 즉시 이동 */
	void SnapToTile(ASATileActor* Tile);

	/** 캐시된 타일 매니저 */
	UPROPERTY(Transient)
	TObjectPtr<USATileManager> CachedTileManager;

	/** 회전/상하 움직임의 기준이 되는 위치 (목표 타일 위) */
	FVector BaseLocation;

	/** 상하 움직임 계산에 사용되는 누적 시간 */
	float ElapsedTime;
};
