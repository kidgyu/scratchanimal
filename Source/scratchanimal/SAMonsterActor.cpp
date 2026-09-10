// Fill out your copyright notice in the Description page of Project Settings.

#include "SAMonsterActor.h"
#include "SATileActor.h"
#include "SATileManager.h"
#include "SAActorManager.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Algo/Reverse.h"
#include "Algo/Sort.h"

ASAMonsterActor::ASAMonsterActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 이동 방향을 바라보는 회전은 이 루트에 적용되고, 메쉬는 애셋 정면 축 보정을 위해 상대 회전을 별도로 가진다
	MonsterRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MonsterRoot"));
	RootComponent = MonsterRoot;

	MonsterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MonsterMesh"));
	MonsterMesh->SetupAttachment(RootComponent);
	MonsterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MonsterMesh->SetCollisionProfileName(TEXT("NoCollision"));
	MonsterMesh->SetGenerateOverlapEvents(false);

	MeshRelativeRotation = FRotator(0.0f, -90.0f, 0.0f); // 애셋 정면 축 보정: 왼쪽으로 90도
	MonsterMesh->SetRelativeRotation(MeshRelativeRotation);

	MeshRelativeLocationOffset = FVector(0.0f, 0.0f, -15.0f); // 애셋 위치 보정: 높이를 15만큼 낮춤
	MonsterMesh->SetRelativeLocation(MeshRelativeLocationOffset);

	// 고정된 단일 몬스터 메쉬(Beholder)를 직접 로드
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BeholderMeshFinder(TEXT("/Game/Monster/Mesh/PBR/Beholder_SK.Beholder_SK"));
	if (BeholderMeshFinder.Succeeded())
	{
		MonsterMesh->SetSkeletalMesh(BeholderMeshFinder.Object);
	}

	// 걷기 애니메이션을 미리 로드해 둔다 (실제 재생은 컴포넌트가 완전히 초기화된 뒤인 BeginPlay에서 시작)
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnimFinder(TEXT("/Game/Monster/Animation/PBR/Beholder/Beholder_WalkFWD_ANIM.Beholder_WalkFWD_ANIM"));
	if (WalkAnimFinder.Succeeded())
	{
		WalkAnimation = WalkAnimFinder.Object;
	}

	TileMoveDuration = 1.0f; // 1초에 1타일씩 이동
	RotationInterpSpeed = 10.0f;
	TileSurfaceZOffset = 15.0f;
	StartDelay = 1.0f; // 임시값
	ChaseRangeFromHome = 5;

	MeshScale = 0.7f;
	MonsterMesh->SetRelativeScale3D(FVector(MeshScale));

	MoveStartLocation = FVector::ZeroVector;
	MoveTargetLocation = FVector::ZeroVector;
	MoveElapsedTime = 0.0f;
	bIsChasing = false;
}

void ASAMonsterActor::BeginPlay()
{
	Super::BeginPlay();

	// 컴포넌트가 완전히 초기화된 이후 재생해야 정상적으로 적용된다 (생성자에서 호출 시 재생되지 않는 문제 방지)
	if (MonsterMesh && WalkAnimation)
	{
		MonsterMesh->PlayAnimation(WalkAnimation, true);
	}

	BindToTileManager();

	// 바인딩 시점에 이미 레벨이 로드되어 시작 타일이 존재한다면(바인딩보다 로드가 먼저 일어난 경우) 즉시 초기화
	if (CachedTileManager && CachedTileManager->GetStartTile())
	{
		HandleLevelLoaded(CachedTileManager->CurrentLevelNumber);
	}
}

void ASAMonsterActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StartDelayTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ASAMonsterActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsChasing)
	{
		return;
	}

	// 카메라 이동 웨이포인트(레벨 16~20)로 화면이 패닝 중이거나 타일 와이프 연출 중이라 터치 입력이
	// 잠겨 있는 동안에는(bInputLocked) 몬스터도 추적을 완전히 멈춘다
	if (CachedTileManager && CachedTileManager->IsInputLocked())
	{
		return;
	}

	// 이동 전에도 한 번 확인 - 정지해 있는 동안 플레이어의 위치(선택 경로)가 지금 서 있는 타일로 바뀌었을 수 있다
	CheckCaughtPlayer();
	if (!bIsChasing)
	{
		return;
	}

	UpdateChase(DeltaTime);
	CheckCaughtPlayer();
}

void ASAMonsterActor::BindToTileManager()
{
	CachedTileManager = USATileManager::Get(this);
	if (CachedTileManager)
	{
		CachedTileManager->OnLevelLoaded.RemoveDynamic(this, &ASAMonsterActor::HandleLevelLoaded);
		CachedTileManager->OnLevelLoaded.AddDynamic(this, &ASAMonsterActor::HandleLevelLoaded);
	}
}

void ASAMonsterActor::HandleLevelLoaded(int32 LevelNumber)
{
	// 새 레벨이 로드되면 이전 타일들은 이미 파괴된 상태이므로 참조를 비우고 추격 상태를 초기화
	CurrentTile = nullptr;
	HomeTile = nullptr;
	MovingToTile = nullptr;
	bIsChasing = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StartDelayTimerHandle);
	}

	if (!CachedTileManager)
	{
		return;
	}

	if (ASATileActor* SpawnTile = FindSpawnTile())
	{
		SnapToTile(SpawnTile);
		HomeTile = SpawnTile; // 이후 추격 범위 판정 및 복귀 목표로 사용할 원위치
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ASAMonsterActor] 몬스터가 스폰될 수 있는 이동 가능 타일을 찾지 못했습니다."));
	}

	// 게임 시작 후 일정 시간(StartDelay)이 지나야 추격을 시작한다
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(StartDelayTimerHandle, this, &ASAMonsterActor::BeginChasing, StartDelay, false);
	}
}

void ASAMonsterActor::BeginChasing()
{
	bIsChasing = true;
}

ASATileActor* ASAMonsterActor::GetPlayerTile() const
{
	if (!CachedTileManager)
	{
		return nullptr;
	}

	// 플레이어가 현재 있는 것으로 간주되는 타일: 드래그 선택 경로의 마지막 칸(=Select 타일), 없으면 시작 타일
	const TArray<ASATileActor*> Path = CachedTileManager->GetSelectedPath();
	return Path.Num() > 0 ? Path.Last() : CachedTileManager->GetStartTile();
}

ASATileActor* ASAMonsterActor::GetChaseTargetTile() const
{
	ASATileActor* PlayerTile = GetPlayerTile();
	ASATileActor* HomeTileActor = HomeTile.Get();

	if (!PlayerTile)
	{
		return HomeTileActor;
	}

	if (HomeTileActor)
	{
		const FIntPoint HomeCoord = HomeTileActor->GetGridCoord();
		const FIntPoint PlayerCoord = PlayerTile->GetGridCoord();
		const int32 ManhattanDist = FMath::Abs(HomeCoord.X - PlayerCoord.X) + FMath::Abs(HomeCoord.Y - PlayerCoord.Y);

		// 원위치 기준 추격 범위를 벗어났다면 플레이어 대신 원위치를 목표로 삼아 복귀시킨다
		if (ManhattanDist > ChaseRangeFromHome)
		{
			return HomeTileActor;
		}
	}

	return PlayerTile;
}

bool ASAMonsterActor::IsTileWalkableForMonster(const ASATileActor* Tile) const
{
	if (!Tile)
	{
		return false;
	}

	const ESATileType Type = Tile->GetTileType();
	return Type != ESATileType::Wall && Type != ESATileType::Start && Type != ESATileType::Goal;
}

TArray<FIntPoint> ASAMonsterActor::FindAutoSpawnCandidateCoords(const FIntPoint& StartCoord) const
{
	static const FIntPoint Directions[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	// 시작 타일에서부터 몬스터가 다닐 수 있는 타일들만 지나서 BFS로 탐색, 거리순으로 후보 목록을 만든다
	TMap<FIntPoint, int32> Distances;
	TArray<FIntPoint> Queue;
	Distances.Add(StartCoord, 0);
	Queue.Add(StartCoord);

	TArray<TPair<int32, FIntPoint>> Candidates; // <거리, 좌표>

	int32 QueueIndex = 0;
	while (QueueIndex < Queue.Num())
	{
		const FIntPoint Current = Queue[QueueIndex++];
		const int32 CurrentDistance = Distances.FindChecked(Current);

		ASATileActor* CurrentTileActor = CachedTileManager->GetTileAtCoord(Current);
		if (Current != StartCoord && CurrentTileActor && IsTileWalkableForMonster(CurrentTileActor))
		{
			Candidates.Add(TPair<int32, FIntPoint>(CurrentDistance, Current));
		}

		for (const FIntPoint& Dir : Directions)
		{
			const FIntPoint Neighbor = Current + Dir;
			if (Distances.Contains(Neighbor))
			{
				continue;
			}

			ASATileActor* NeighborTile = CachedTileManager->GetTileAtCoord(Neighbor);
			if (!NeighborTile)
			{
				continue;
			}

			// 시작 타일 자신을 통해서는 지나갈 수 있게 해서 연결된 구역 전체를 탐색하되, 시작/도착/벽 타일 자체는 스폰 후보에서 제외한다
			if (Neighbor != StartCoord && !IsTileWalkableForMonster(NeighborTile))
			{
				continue;
			}

			Distances.Add(Neighbor, CurrentDistance + 1);
			Queue.Add(Neighbor);
		}
	}

	// 거리가 먼 순서대로 정렬
	Algo::SortBy(Candidates, [](const TPair<int32, FIntPoint>& Pair) { return Pair.Key; }, TGreater<int32>());

	TArray<FIntPoint> OrderedCoords;
	OrderedCoords.Reserve(Candidates.Num());
	for (const TPair<int32, FIntPoint>& Candidate : Candidates)
	{
		OrderedCoords.Add(Candidate.Value);
	}
	return OrderedCoords;
}

ASATileActor* ASAMonsterActor::FindSpawnTile() const
{
	if (!CachedTileManager)
	{
		return nullptr;
	}

	ASATileActor* StartTile = CachedTileManager->GetStartTile();
	if (!StartTile)
	{
		return nullptr;
	}

	// 레벨 데이터에 몬스터별 지정 스폰 좌표가 있으면 그 순서를 우선 후보로 사용하고,
	// 없으면 시작 타일에서 가장 멀리 떨어진 타일부터 사용하는 기존 자동 계산으로 대체한다
	TArray<FIntPoint> OrderedCandidateCoords = CachedTileManager->CurrentLevelData.MonsterSpawnCoords;
	if (OrderedCandidateCoords.IsEmpty())
	{
		OrderedCandidateCoords = FindAutoSpawnCandidateCoords(StartTile->GetGridCoord());
	}

	if (OrderedCandidateCoords.IsEmpty())
	{
		return nullptr;
	}

	// 다른 몬스터가 이미 차지하고 있는 타일은 피해서, 순서상 가장 앞서고 비어있는 첫 번째 타일을 스폰 위치로 삼는다
	TSet<ASATileActor*> OccupiedByOtherMonsters;
	if (USAActorManager* ActorManager = USAActorManager::Get(this))
	{
		for (ASAMonsterActor* OtherMonster : ActorManager->GetMonsterActors())
		{
			if (OtherMonster && OtherMonster != this)
			{
				if (ASATileActor* OtherTile = OtherMonster->GetCurrentTile())
				{
					OccupiedByOtherMonsters.Add(OtherTile);
				}
			}
		}
	}

	ASATileActor* FirstWalkableFallback = nullptr;
	for (const FIntPoint& Coord : OrderedCandidateCoords)
	{
		ASATileActor* CandidateTile = CachedTileManager->GetTileAtCoord(Coord);
		if (!CandidateTile || !IsTileWalkableForMonster(CandidateTile))
		{
			continue;
		}

		if (!FirstWalkableFallback)
		{
			FirstWalkableFallback = CandidateTile;
		}

		if (!OccupiedByOtherMonsters.Contains(CandidateTile))
		{
			return CandidateTile;
		}
	}

	// 모든 후보가 다른 몬스터로 채워져 있거나(그리드가 매우 작은 경우), 지정 좌표 중 걸을 수 있는 타일이 하나뿐이라면 그대로 사용
	return FirstWalkableFallback;
}

TArray<ASATileActor*> ASAMonsterActor::FindPathAStar(ASATileActor* From, ASATileActor* To) const
{
	TArray<ASATileActor*> ResultPath;

	if (!CachedTileManager || !From || !To)
	{
		return ResultPath;
	}

	// 목표 타일 자체가 몬스터가 밟을 수 없는 타일(예: 플레이어가 아직 시작 타일에 있는 경우)이라면,
	// 그 타일에 인접한 이동 가능한 타일까지만 접근하도록 임시 목표를 대신 사용한다
	ASATileActor* AdjustedGoalTile = To;
	static const FIntPoint Directions[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	if (!IsTileWalkableForMonster(AdjustedGoalTile))
	{
		AdjustedGoalTile = nullptr;
		for (const FIntPoint& Dir : Directions)
		{
			ASATileActor* NeighborTile = CachedTileManager->GetTileAtCoord(To->GetGridCoord() + Dir);
			if (IsTileWalkableForMonster(NeighborTile))
			{
				AdjustedGoalTile = NeighborTile;
				break;
			}
		}

		if (!AdjustedGoalTile)
		{
			return ResultPath; // 접근 가능한 인접 타일조차 없음
		}
	}

	const FIntPoint StartCoord = From->GetGridCoord();
	const FIntPoint GoalCoord = AdjustedGoalTile->GetGridCoord();

	if (StartCoord == GoalCoord)
	{
		return ResultPath;
	}

	auto Heuristic = [](const FIntPoint& A, const FIntPoint& B)
	{
		return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
	};

	TSet<FIntPoint> OpenSet;
	TSet<FIntPoint> ClosedSet;
	TMap<FIntPoint, FIntPoint> CameFrom;
	TMap<FIntPoint, int32> GScore;
	TMap<FIntPoint, int32> FScore;

	OpenSet.Add(StartCoord);
	GScore.Add(StartCoord, 0);
	FScore.Add(StartCoord, Heuristic(StartCoord, GoalCoord));

	while (OpenSet.Num() > 0)
	{
		// OpenSet 중 FScore가 가장 낮은 노드를 선택 (그리드 규모가 작아 별도 우선순위 큐 없이 선형 탐색으로 충분)
		FIntPoint Current = *OpenSet.CreateConstIterator();
		int32 BestFScore = FScore.FindRef(Current);
		for (const FIntPoint& Candidate : OpenSet)
		{
			const int32 CandidateFScore = FScore.FindRef(Candidate);
			if (CandidateFScore < BestFScore)
			{
				Current = Candidate;
				BestFScore = CandidateFScore;
			}
		}

		if (Current == GoalCoord)
		{
			// 목표에서부터 CameFrom을 거슬러 올라가며 경로를 역추적한 뒤 순서를 뒤집는다
			TArray<FIntPoint> CoordPath;
			FIntPoint Trace = Current;
			while (Trace != StartCoord)
			{
				CoordPath.Add(Trace);
				Trace = CameFrom.FindChecked(Trace);
			}
			Algo::Reverse(CoordPath);

			for (const FIntPoint& Coord : CoordPath)
			{
				if (ASATileActor* Tile = CachedTileManager->GetTileAtCoord(Coord))
				{
					ResultPath.Add(Tile);
				}
			}
			return ResultPath;
		}

		OpenSet.Remove(Current);
		ClosedSet.Add(Current);

		for (const FIntPoint& Dir : Directions)
		{
			const FIntPoint Neighbor = Current + Dir;
			if (ClosedSet.Contains(Neighbor))
			{
				continue;
			}

			ASATileActor* NeighborTile = CachedTileManager->GetTileAtCoord(Neighbor);
			if (!IsTileWalkableForMonster(NeighborTile))
			{
				continue;
			}

			const int32 TentativeGScore = GScore.FindRef(Current) + 1;
			if (!GScore.Contains(Neighbor) || TentativeGScore < GScore.FindRef(Neighbor))
			{
				CameFrom.Add(Neighbor, Current);
				GScore.Add(Neighbor, TentativeGScore);
				FScore.Add(Neighbor, TentativeGScore + Heuristic(Neighbor, GoalCoord));
				OpenSet.Add(Neighbor);
			}
		}
	}

	return ResultPath; // 경로 없음 (비어있는 배열)
}

void ASAMonsterActor::StartMovingToTile(ASATileActor* NextTile)
{
	if (!NextTile)
	{
		return;
	}

	MovingToTile = NextTile;
	MoveStartLocation = GetActorLocation();
	MoveTargetLocation = NextTile->GetActorLocation() + FVector(0.0f, 0.0f, TileSurfaceZOffset);
	MoveElapsedTime = 0.0f;
}

void ASAMonsterActor::UpdateChase(float DeltaTime)
{
	if (!CachedTileManager || !CurrentTile.IsValid())
	{
		return;
	}

	if (!MovingToTile.IsValid())
	{
		// 정지 상태: 현재 목표(플레이어 위치)까지의 다음 한 칸을 A*로 계산해서 이동을 시작한다
		ASATileActor* TargetTile = GetChaseTargetTile();
		if (!TargetTile || TargetTile == CurrentTile.Get())
		{
			return;
		}

		const TArray<ASATileActor*> Path = FindPathAStar(CurrentTile.Get(), TargetTile);
		if (Path.Num() > 0)
		{
			StartMovingToTile(Path[0]);
		}
		return;
	}

	// 이동 중: 목표 위치까지 보간 진행
	MoveElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(MoveElapsedTime / TileMoveDuration, 0.0f, 1.0f);
	const FVector CurrentLocation = GetActorLocation();
	const FVector NewLocation = FMath::Lerp(MoveStartLocation, MoveTargetLocation, Alpha);
	const FVector MoveDelta = NewLocation - CurrentLocation;

	// 이동하는 방향으로 바라보도록 회전 보간 (SAAnimalActor와 동일한 방식)
	if (!MoveDelta.IsNearlyZero())
	{
		const FRotator DesiredRotation = MoveDelta.GetSafeNormal().Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaTime, RotationInterpSpeed));
	}

	SetActorLocation(NewLocation);

	if (Alpha >= 1.0f)
	{
		ASATileActor* ArrivedTile = MovingToTile.Get();
		MovingToTile = nullptr;

		if (ArrivedTile && CurrentTile.Get() != ArrivedTile)
		{
			if (ASATileActor* PreviousTile = CurrentTile.Get())
			{
				PreviousTile->SetMonsterOccupied(false);
			}
			CurrentTile = ArrivedTile;
			ArrivedTile->SetMonsterOccupied(true);
		}
	}
}

void ASAMonsterActor::SnapToTile(ASATileActor* Tile)
{
	if (!Tile)
	{
		return;
	}

	if (ASATileActor* PreviousTile = CurrentTile.Get())
	{
		if (PreviousTile != Tile)
		{
			PreviousTile->SetMonsterOccupied(false);
		}
	}

	CurrentTile = Tile;
	MovingToTile = nullptr;
	Tile->SetMonsterOccupied(true);

	MoveTargetLocation = Tile->GetActorLocation() + FVector(0.0f, 0.0f, TileSurfaceZOffset);
	MoveStartLocation = MoveTargetLocation;
	MoveElapsedTime = 0.0f;
	SetActorLocation(MoveTargetLocation);
}

void ASAMonsterActor::CheckCaughtPlayer()
{
	if (!CachedTileManager || !CurrentTile.IsValid())
	{
		return;
	}

	// 포획 판정은 항상 실제 플레이어 위치 기준으로 확인한다 (추격 범위를 벗어나 원위치로 복귀 중일 때
	// CurrentTile이 우연히 HomeTile과 같아지는 것을 오탐으로 처리하지 않기 위함)
	ASATileActor* PlayerTile = GetPlayerTile();
	if (PlayerTile && PlayerTile == CurrentTile.Get())
	{
		// 더 이상 진행하지 않도록 추격을 멈추고, 결과(Lose) 전환은 SATileManager/SAGameState에 위임한다
		bIsChasing = false;
		CachedTileManager->NotifyMonsterCaughtPlayer();
	}
}
