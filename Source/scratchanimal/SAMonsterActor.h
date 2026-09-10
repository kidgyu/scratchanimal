// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SAMonsterActor.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class ASATileActor;
class USATileManager;

/**
 * 벽/시작/도착 타일을 제외한 모든 타일을 A* 알고리즘으로 길찾기하며, 플레이어가 현재 있는 것으로
 * 간주되는 타일(드래그 선택 경로의 마지막 칸, 경로가 없으면 시작 타일)을 추격하는 몬스터 액터.
 * 스폰 위치(원위치) 기준 상하좌우 ChaseRangeFromHome 칸 이내에 플레이어가 들어왔을 때만 추격하고,
 * 범위를 벗어나면 다시 원위치로 되돌아간다. 콜라이더 없이 그리드 좌표 비교만으로 추격/포획을
 * 판정하며(포획 판정은 항상 실제 플레이어 위치 기준), 붙잡으면 SATileManager를 통해 Result Lose로
 * 전환시킨다. 자신이 서 있는 타일은 타입을 바꾸지 않은 채 바닥색만 임시로 진한 보라색으로 표시한다.
 */
UCLASS()
class SCRATCHANIMAL_API ASAMonsterActor : public AActor
{
	GENERATED_BODY()

public:
	ASAMonsterActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// 컴포넌트
	// -------------------------------------------------------------

	/** 이동/회전의 기준이 되는 루트 컴포넌트 (콜라이더 없음) - 이동 방향을 바라보는 회전은 이 컴포넌트에 적용된다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> MonsterRoot;

	/**
	 * 몬스터 외형 스켈레탈 메쉬. 애셋(Beholder)의 정면 축이 액터 정면과 어긋나 있어, 이동 방향과
	 * 무관하게 항상 MeshRelativeRotation만큼 보정된 상태로 MonsterRoot에 부착되어 있다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> MonsterMesh;

	// -------------------------------------------------------------
	// 이동 설정
	// -------------------------------------------------------------

	/** 한 칸을 이동하는 데 걸리는 시간(초) - "1초에 1타일씩 이동"에 대응 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Movement", meta = (ClampMin = "0.01"))
	float TileMoveDuration;

	/** 이동 방향으로 회전할 때의 보간 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Movement", meta = (ClampMin = "0.1"))
	float RotationInterpSpeed;

	/** 타일 중심 기준으로 얼마나 위(Z)에 올려놓을지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Movement")
	float TileSurfaceZOffset;

	/** 레벨이 시작된 뒤 실제로 추격(이동)을 시작하기까지의 대기 시간(초). 임시값 1초 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Movement", meta = (ClampMin = "0.0"))
	float StartDelay;

	/**
	 * 스폰 위치(원위치) 기준 상하좌우로 몇 칸 이내에 플레이어가 들어와야 추격을 시작할지 (맨해튼 거리).
	 * 플레이어가 이 범위를 벗어나면 추격을 멈추고 원위치로 되돌아간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Movement", meta = (ClampMin = "0"))
	int32 ChaseRangeFromHome;

	// -------------------------------------------------------------
	// 메쉬 설정
	// -------------------------------------------------------------

	/** 메쉬 스케일 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Mesh")
	float MeshScale;

	/** 메쉬 애셋의 정면 축 보정용 상대 회전 (기본: 왼쪽으로 90도) - MonsterRoot 기준 MonsterMesh에 고정 적용됨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Mesh")
	FRotator MeshRelativeRotation;

	/** 메쉬 위치 보정값 (기본: Z를 15만큼 낮춤) - MonsterRoot 기준 MonsterMesh에 고정 적용됨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Mesh")
	FVector MeshRelativeLocationOffset;

	/** 현재 이 몬스터가 서 있는(마지막으로 도착 완료한) 타일 반환 (다른 몬스터가 스폰 위치를 겹치지 않게 고를 때 사용) */
	UFUNCTION(BlueprintPure, Category = "Monster")
	ASATileActor* GetCurrentTile() const { return CurrentTile.Get(); }

private:
	/** SATileManager의 이벤트에 바인딩해서 레벨이 새로 로드될 때마다 스폰 위치를 다시 잡고 추격을 재시작하도록 한다 */
	void BindToTileManager();

	UFUNCTION()
	void HandleLevelLoaded(int32 LevelNumber);

	/** StartDelay가 끝나면 호출되어 실제 추격(이동)을 시작한다 */
	void BeginChasing();

	/** 현재 플레이어가 있는 것으로 간주되는 타일(선택 경로의 마지막 칸, 없으면 시작 타일)을 반환 */
	ASATileActor* GetPlayerTile() const;

	/**
	 * 실제 추격 목표 타일을 반환한다. 플레이어 타일이 스폰 위치(HomeTile) 기준 ChaseRangeFromHome 칸
	 * 이내에 있으면 플레이어 타일을, 범위를 벗어나면 HomeTile(원위치)을 목표로 삼는다.
	 */
	ASATileActor* GetChaseTargetTile() const;

	/** 벽/시작/도착 타일을 제외하고 몬스터가 다닐 수 있는 타일인지 여부 */
	bool IsTileWalkableForMonster(const ASATileActor* Tile) const;

	/**
	 * 레벨 데이터에 지정된 몬스터 스폰 좌표가 있으면 그 순서대로, 없으면 시작 타일에서 가장 멀리
	 * 떨어진 타일부터 사용해 초기 스폰 위치를 찾는다. 다른 몬스터가 이미 차지한 타일은 건너뛴다.
	 */
	ASATileActor* FindSpawnTile() const;

	/** 시작 좌표에서부터 몬스터가 다닐 수 있는 타일들만 BFS로 탐색해, 거리가 먼 순서대로 정렬된 좌표 목록을 반환 (FindSpawnTile의 자동 대체 후보 계산용) */
	TArray<FIntPoint> FindAutoSpawnCandidateCoords(const FIntPoint& StartCoord) const;

	/** From에서 To까지, 몬스터가 다닐 수 있는 타일만 지나서 가는 최단 경로를 A*로 계산한다 (From 자신은 포함하지 않음) */
	TArray<ASATileActor*> FindPathAStar(ASATileActor* From, ASATileActor* To) const;

	/** 다음 한 칸으로의 이동을 시작 (보간 구간 초기화) */
	void StartMovingToTile(ASATileActor* NextTile);

	/** 이동 중이면 목표 위치까지 보간을 진행시키고, 정지 상태면 다음 목표 칸을 A*로 계산해서 이동을 시작시킨다 */
	void UpdateChase(float DeltaTime);

	/** 지정된 타일의 위치(+ Z 오프셋)로 보간 없이 즉시 이동. 이전/새 점유 타일의 보라색 표시도 함께 갱신한다 */
	void SnapToTile(ASATileActor* Tile);

	/** 현재 서 있는 타일이 플레이어가 있는 타일과 같은지 확인하고, 같으면 SATileManager를 통해 포획 이벤트를 발생시킨다 */
	void CheckCaughtPlayer();

	/** 생성자에서 미리 로드해 둔 걷기 애니메이션 (BeginPlay에서 재생 시작) */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkAnimation;

	/** 캐시된 타일 매니저 */
	UPROPERTY(Transient)
	TObjectPtr<USATileManager> CachedTileManager;

	/** 현재 몬스터가 서 있는(마지막으로 도착 완료한) 타일 */
	TWeakObjectPtr<ASATileActor> CurrentTile;

	/** 스폰된(원위치) 타일 - 추격 범위 판정 및 범위를 벗어났을 때 복귀할 목표로 사용 */
	TWeakObjectPtr<ASATileActor> HomeTile;

	/** 현재 이동 중인 목표 타일 (정지 상태면 유효하지 않음) */
	TWeakObjectPtr<ASATileActor> MovingToTile;

	/** 현재 이동 구간의 시작 위치 */
	FVector MoveStartLocation;

	/** 현재 이동 구간의 목표 위치 */
	FVector MoveTargetLocation;

	/** 현재 이동 구간이 시작된 뒤 흐른 시간(초) */
	float MoveElapsedTime;

	/** StartDelay가 끝나 실제로 추격(이동)을 시작했는지 여부 */
	bool bIsChasing;

	/** 대기 시간(StartDelay) 타이머 핸들 */
	FTimerHandle StartDelayTimerHandle;
};
