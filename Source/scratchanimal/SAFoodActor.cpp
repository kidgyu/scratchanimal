// Fill out your copyright notice in the Description page of Project Settings.

#include "SAFoodActor.h"
#include "SATileActor.h"
#include "SATileManager.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ASAFoodActor::ASAFoodActor()
{
	PrimaryActorTick.bCanEverTick = true;

	FoodMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FoodMesh"));
	RootComponent = FoodMesh;
	FoodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FoodMesh->SetCollisionProfileName(TEXT("NoCollision"));
	FoodMesh->SetGenerateOverlapEvents(false);
	FoodMesh->SetRelativeScale3D(FVector(5.0f, 5.0f, 5.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BurgerMeshFinder(TEXT("/Game/StylizedBurger/Meshes/SM_Burger_Double.SM_Burger_Double"));
	if (BurgerMeshFinder.Succeeded())
	{
		FoodMesh->SetStaticMesh(BurgerMeshFinder.Object);
	}

	RotationSpeed = 30.0f;
	BobDistance = 20.0f;
	BobSpeed = 1.0f;
	TileSurfaceZOffset = 15.0f;

	BaseLocation = FVector::ZeroVector;
	ElapsedTime = 0.0f;
}

void ASAFoodActor::BeginPlay()
{
	Super::BeginPlay();

	BaseLocation = GetActorLocation();

	BindToTileManager();

	// 이미 레벨이 로드되어 목표 타일이 존재한다면(바인딩보다 로드가 먼저 일어난 경우) 즉시 그 위치로 이동
	if (CachedTileManager)
	{
		if (ASATileActor* GoalTile = CachedTileManager->GetGoalTile())
		{
			SnapToTile(GoalTile);
		}
	}
}

void ASAFoodActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedTime += DeltaTime;

	// 제자리 회전
	AddActorLocalRotation(FRotator(0.0f, RotationSpeed * DeltaTime, 0.0f));

	// 기준 위치를 중심으로 위아래로 부드럽게 왕복 (최고점-최저점 차이 = BobDistance)
	FVector NewLocation = BaseLocation;
	NewLocation.Z += FMath::Sin(ElapsedTime * BobSpeed) * (BobDistance * 0.5f);
	SetActorLocation(NewLocation);
}

void ASAFoodActor::BindToTileManager()
{
	CachedTileManager = USATileManager::Get(this);
	if (CachedTileManager)
	{
		CachedTileManager->OnLevelLoaded.AddDynamic(this, &ASAFoodActor::HandleLevelLoaded);
	}
}

void ASAFoodActor::HandleLevelLoaded(int32 LevelNumber)
{
	if (!CachedTileManager)
	{
		return;
	}

	// 새 레벨이 로드되면 이전 타일들은 이미 파괴된 상태이므로, 새 목표 타일 위치로 즉시 이동
	if (ASATileActor* GoalTile = CachedTileManager->GetGoalTile())
	{
		SnapToTile(GoalTile);
	}
}

void ASAFoodActor::SnapToTile(ASATileActor* Tile)
{
	if (!Tile)
	{
		return;
	}

	BaseLocation = Tile->GetActorLocation() + FVector(0.0f, 0.0f, TileSurfaceZOffset);
	SetActorLocation(BaseLocation);
}
