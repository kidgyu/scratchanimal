// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SAAnimalActor.generated.h"

class USkeletalMeshComponent;
class ASATileActor;
class USATileManager;

/**
 * 플레이어의 드래그 경로를 시각적으로 따라다니는 동물 액터.
 * 콜라이더 없이, 항상 "마지막으로 선택된 타일"(경로가 비어 있으면 시작 타일) 위치로 이동/회전하며,
 * 시작 시 지정된 폴더에서 SK_ 접두어 스켈레탈 메쉬 하나를 무작위로 골라 게임이 끝날 때까지 유지한다.
 */
UCLASS()
class SCRATCHANIMAL_API ASAAnimalActor : public AActor
{
	GENERATED_BODY()

public:
	ASAAnimalActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// 컴포넌트
	// -------------------------------------------------------------

	/** 동물 외형 스켈레탈 메쉬 (콜라이더 없이 루트 컴포넌트로 사용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> AnimalMesh;

	// -------------------------------------------------------------
	// 이동 설정
	// -------------------------------------------------------------

	/** 목표 타일이 바뀔 때마다, 그 위치까지 도달하는 데 걸리는 시간(초) - 거리와 상관없이 항상 이 시간 안에 도착 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal|Movement", meta = (ClampMin = "0.01"))
	float MoveDuration;

	/** 이동 방향으로 회전할 때의 보간 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal|Movement", meta = (ClampMin = "0.1"))
	float RotationInterpSpeed;

	/** 타일 중심 기준으로 얼마나 위(Z)에 올려놓을지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal|Movement")
	float TileSurfaceZOffset;

	// -------------------------------------------------------------
	// 메쉬 랜덤 로딩 설정
	// -------------------------------------------------------------

	/** 메쉬 스케일 (원본 모델링 크기가 너무 커서 축소해서 사용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal|Mesh")
	float MeshScale;

	/** 랜덤 메쉬를 검색할 폴더 경로 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal|Mesh")
	FString MeshSearchPath;

	/** 검색 대상 메쉬 이름 접두어 (예: SK_Bunny, SK_Cat, SK_Chick...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal|Mesh")
	FString MeshNamePrefix;

	/** 걷기 애니메이션(Anim_xxx_Walk)을 검색할 폴더 경로 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal|Mesh")
	FString AnimSearchPath;

private:
	/** MeshSearchPath 폴더에서 MeshNamePrefix로 시작하는 스켈레탈 메쉬 중 하나를 무작위로 찾아 1회 적용 (이후 절대 바뀌지 않음) */
	void LoadRandomAnimalMesh();

	/**
	 * 로드된 메쉬 이름(예: SK_Bunny)에서 접두어를 뗀 이름으로 "Anim_Bunny_Walk"를 찾아 재생한다.
	 * 해당 동물 전용 Walk 애니메이션이 없으면(예: 새 종류처럼 Walk 대신 Fly류만 있는 경우) 공용 "Anim_Animal_Walk"로 대체한다.
	 */
	void ApplyWalkAnimationForMesh(const FString& MeshAssetName);

	/** SATileManager의 이벤트에 바인딩해서 레벨이 새로 로드될 때 시작 타일로 즉시 이동하도록 한다 */
	void BindToTileManager();

	UFUNCTION()
	void HandleLevelLoaded(int32 LevelNumber);

	/** 매 프레임 SATileManager의 선택 경로를 확인해 목표 타일(마지막 선택 타일, 없으면 시작 타일)이 바뀌었는지 갱신 */
	void RefreshTargetFromSelectedPath();

	/** 목표 위치를 향해 이동하고, 이동 방향으로 회전을 보간 */
	void MoveTowardsTarget(float DeltaTime);

	/** 지정된 타일의 위치(+ Z 오프셋)로 보간 없이 즉시 이동 (레벨 로드 시 사용) */
	void SnapToTile(ASATileActor* Tile);

	/** 캐시된 타일 매니저 */
	UPROPERTY(Transient)
	TObjectPtr<USATileManager> CachedTileManager;

	/** 현재 이동 목표로 삼고 있는 타일 (마지막으로 선택된 타일, 없으면 시작 타일) */
	TWeakObjectPtr<ASATileActor> CurrentTargetTile;

	/** 현재 이동 목표 월드 위치 */
	FVector TargetLocation;

	/** 현재 이동 구간이 시작된 시점의 위치 (Lerp의 시작점) */
	FVector MoveStartLocation;

	/** 현재 이동 구간이 시작된 뒤 흐른 시간(초) */
	float MoveElapsedTime;
};
