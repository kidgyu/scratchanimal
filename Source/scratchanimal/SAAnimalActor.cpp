// Fill out your copyright notice in the Description page of Project Settings.

#include "SAAnimalActor.h"
#include "SATileActor.h"
#include "SATileManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"

ASAAnimalActor::ASAAnimalActor()
{
	PrimaryActorTick.bCanEverTick = true;

	AnimalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimalMesh"));
	RootComponent = AnimalMesh;
	AnimalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimalMesh->SetCollisionProfileName(TEXT("NoCollision"));
	AnimalMesh->SetGenerateOverlapEvents(false);

	MoveDuration = 0.2f;
	RotationInterpSpeed = 10.0f;
	TileSurfaceZOffset = 15.0f;

	MeshScale = 0.5f;
	AnimalMesh->SetRelativeScale3D(FVector(MeshScale)); // 원본 모델링 크기가 너무 커서 축소

	MeshSearchPath = TEXT("/Game/ANIMMAL/KUBIKOS/Models"); // 실제 애셋 폴더명이 "ANIMAL"이 아니라 "ANIMMAL"(M 두 개)임에 주의
	MeshNamePrefix = TEXT("SK_");
	AnimSearchPath = TEXT("/Game/ANIMMAL/KUBIKOS/Animations");

	TargetLocation = FVector::ZeroVector;
	MoveStartLocation = FVector::ZeroVector;
	MoveElapsedTime = 0.0f;
}

void ASAAnimalActor::BeginPlay()
{
	Super::BeginPlay();

	// 게임이 끝날 때까지 유지할 랜덤 동물 메쉬를 이 시점에 단 한 번만 결정
	LoadRandomAnimalMesh();

	BindToTileManager();

	// 이미 레벨이 로드되어 시작 타일이 존재한다면(바인딩보다 로드가 먼저 일어난 경우) 즉시 그 위치로 이동
	if (CachedTileManager)
	{
		if (ASATileActor* StartTile = CachedTileManager->GetStartTile())
		{
			SnapToTile(StartTile);
		}
	}
}

void ASAAnimalActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshTargetFromSelectedPath();
	MoveTowardsTarget(DeltaTime);
}

void ASAAnimalActor::LoadRandomAnimalMesh()
{
	if (!AnimalMesh)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*MeshSearchPath));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> FoundAssets;
	AssetRegistry.GetAssets(Filter, FoundAssets);

	TArray<FAssetData> Candidates;
	for (const FAssetData& AssetData : FoundAssets)
	{
		if (AssetData.AssetName.ToString().StartsWith(MeshNamePrefix))
		{
			Candidates.Add(AssetData);
		}
	}

	if (Candidates.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ASAAnimalActor] '%s' 경로에서 '%s' 접두어 스켈레탈 메쉬를 찾지 못했습니다."), *MeshSearchPath, *MeshNamePrefix);
		return;
	}

	const int32 RandomIndex = FMath::RandRange(0, Candidates.Num() - 1);
	const FAssetData& ChosenAsset = Candidates[RandomIndex];
	if (USkeletalMesh* RandomMesh = Cast<USkeletalMesh>(ChosenAsset.GetAsset()))
	{
		AnimalMesh->SetSkeletalMesh(RandomMesh);
		ApplyWalkAnimationForMesh(ChosenAsset.AssetName.ToString());
	}
}

void ASAAnimalActor::ApplyWalkAnimationForMesh(const FString& MeshAssetName)
{
	if (!AnimalMesh)
	{
		return;
	}

	// "SK_Bunny" -> "Bunny"
	FString AnimalName = MeshAssetName;
	if (AnimalName.StartsWith(MeshNamePrefix))
	{
		AnimalName.RightChopInline(MeshNamePrefix.Len());
	}

	const FString DesiredWalkName = FString::Printf(TEXT("Anim_%s_Walk"), *AnimalName);
	static const FString FallbackWalkName = TEXT("Anim_Animal_Walk");

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*AnimSearchPath));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> FoundAssets;
	AssetRegistry.GetAssets(Filter, FoundAssets);

	const FAssetData* FoundAsset = FoundAssets.FindByPredicate([&DesiredWalkName](const FAssetData& AssetData)
	{
		return AssetData.AssetName.ToString() == DesiredWalkName;
	});

	if (!FoundAsset)
	{
		// 새(조류)류처럼 전용 Walk 애니메이션이 없는 동물은 공용 Anim_Animal_Walk로 대체
		UE_LOG(LogTemp, Warning, TEXT("[ASAAnimalActor] '%s' 애니메이션을 찾지 못해 공용 '%s'로 대체합니다."), *DesiredWalkName, *FallbackWalkName);
		FoundAsset = FoundAssets.FindByPredicate([](const FAssetData& AssetData)
		{
			return AssetData.AssetName.ToString() == FallbackWalkName;
		});
	}

	if (!FoundAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ASAAnimalActor] '%s' 경로에서 걷기 애니메이션을 전혀 찾지 못했습니다."), *AnimSearchPath);
		return;
	}

	if (UAnimSequence* WalkAnim = Cast<UAnimSequence>(FoundAsset->GetAsset()))
	{
		AnimalMesh->PlayAnimation(WalkAnim, true);
	}
}

void ASAAnimalActor::BindToTileManager()
{
	CachedTileManager = USATileManager::Get(this);
	if (CachedTileManager)
	{
		CachedTileManager->OnLevelLoaded.AddDynamic(this, &ASAAnimalActor::HandleLevelLoaded);
	}
}

void ASAAnimalActor::HandleLevelLoaded(int32 LevelNumber)
{
	if (!CachedTileManager)
	{
		return;
	}

	// 새 레벨이 로드되면 이전 타일들은 이미 파괴된 상태이므로, 슬라이드 이동 없이 새 시작 타일로 즉시 이동
	if (ASATileActor* StartTile = CachedTileManager->GetStartTile())
	{
		SnapToTile(StartTile);
	}
}

void ASAAnimalActor::RefreshTargetFromSelectedPath()
{
	if (!CachedTileManager)
	{
		return;
	}

	const TArray<ASATileActor*> Path = CachedTileManager->GetSelectedPath();
	ASATileActor* DesiredTile = Path.Num() > 0 ? Path.Last() : CachedTileManager->GetStartTile();

	if (!DesiredTile)
	{
		return;
	}

	if (CurrentTargetTile.Get() != DesiredTile)
	{
		CurrentTargetTile = DesiredTile;
		TargetLocation = DesiredTile->GetActorLocation() + FVector(0.0f, 0.0f, TileSurfaceZOffset);

		// 새 목표가 잡힌 지금 위치부터 MoveDuration 안에 도착하도록 이동 구간을 새로 시작
		MoveStartLocation = GetActorLocation();
		MoveElapsedTime = 0.0f;
	}
}

void ASAAnimalActor::MoveTowardsTarget(float DeltaTime)
{
	if (!CurrentTargetTile.IsValid())
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	if (CurrentLocation.Equals(TargetLocation, 0.5f))
	{
		return;
	}

	MoveElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(MoveElapsedTime / MoveDuration, 0.0f, 1.0f);
	const FVector NewLocation = FMath::Lerp(MoveStartLocation, TargetLocation, Alpha);
	const FVector MoveDelta = NewLocation - CurrentLocation;

	// 드래그로 이동하는 방향을 바라보도록 회전 (그리드가 축 정렬이라 자연스럽게 4방향 중 하나를 향하게 됨)
	if (!MoveDelta.IsNearlyZero())
	{
		const FRotator DesiredRotation = MoveDelta.GetSafeNormal().Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaTime, RotationInterpSpeed));
	}

	SetActorLocation(NewLocation);
}

void ASAAnimalActor::SnapToTile(ASATileActor* Tile)
{
	if (!Tile)
	{
		return;
	}

	CurrentTargetTile = Tile;
	TargetLocation = Tile->GetActorLocation() + FVector(0.0f, 0.0f, TileSurfaceZOffset);
	MoveStartLocation = TargetLocation;
	MoveElapsedTime = 0.0f;
	SetActorLocation(TargetLocation);
}
