// Fill out your copyright notice in the Description page of Project Settings.

#include "SAResultTileActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ASAResultTileActor::ASAResultTileActor()
{
	PrimaryActorTick.bCanEverTick = false;

	GridCoord = FIntPoint::ZeroValue;
	bIsPatternLit = false;
	bIsDraggable = false;
	bIsSelected = false;

	BackgroundColor = FLinearColor(0.4f, 0.85f, 0.4f, 1.0f);    // 패턴 꺼짐 (SATileActor의 Normal과 동일한 옅은 초록색)
	PatternLitColor = FLinearColor(0.9f, 0.9f, 0.95f, 1.0f);    // 패턴 켜짐 (기본값, ShowResult에서 승/패에 따라 덮어씀)
	DraggableColor = FLinearColor(0.1f, 0.85f, 0.2f, 1.0f);     // 슬라이더 타일 (SATileActor의 Start와 동일한 초록색)
	SelectedColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f);       // 드래그 중 (SATileActor의 Selected와 동일한 빨간색)

	// 1. 충돌 콜라이더 생성 (RootComponent)
	BoxCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollider"));
	RootComponent = BoxCollider;
	BoxCollider->SetBoxExtent(FVector(50.f, 50.f, 10.f)); // 100 x 100 x 20 기본 타일 크기
	BoxCollider->SetCollisionProfileName(TEXT("BlockAll"));
	BoxCollider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoxCollider->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BoxCollider->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	BoxCollider->SetGenerateOverlapEvents(true);

	// 2. 메쉬 컴포넌트 생성 및 콜라이더에 부착
	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	TileMesh->SetupAttachment(RootComponent);
	TileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 충돌은 BoxCollider가 전담

	// 언리얼 기본 큐브 메쉬 로드
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		TileMesh->SetStaticMesh(CubeMeshFinder.Object);
		// 100x100x20 크기의 납작한 바닥 타일 형태로 스케일 조정 (Cube 기본은 100x100x100)
		TileMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.2f));
		TileMesh->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	}

	// 기본 머티리얼 로드
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (DefaultMatFinder.Succeeded())
	{
		TileMesh->SetMaterial(0, DefaultMatFinder.Object);
	}
}

void ASAResultTileActor::BeginPlay()
{
	Super::BeginPlay();

	EnsureDynamicMaterial();
	UpdateVisual();
}

#if WITH_EDITOR
void ASAResultTileActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	EnsureDynamicMaterial();
	UpdateVisual();
}
#endif

void ASAResultTileActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASAResultTileActor::SetGridCoord(int32 InX, int32 InY)
{
	GridCoord = FIntPoint(InX, InY);
}

void ASAResultTileActor::SetPatternLit(bool bInLit)
{
	bIsPatternLit = bInLit;
	UpdateVisual();
}

void ASAResultTileActor::SetDraggable(bool bInDraggable)
{
	bIsDraggable = bInDraggable;
	UpdateVisual();
}

void ASAResultTileActor::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		bIsSelected = bInSelected;
		UpdateVisual();
	}
}

FLinearColor ASAResultTileActor::GetColorForCurrentState() const
{
	if (bIsSelected)
	{
		return SelectedColor;
	}

	if (bIsDraggable)
	{
		return DraggableColor;
	}

	if (bIsPatternLit)
	{
		return PatternLitColor;
	}

	return BackgroundColor;
}

void ASAResultTileActor::EnsureDynamicMaterial()
{
	if (!DynamicMaterial && TileMesh)
	{
		DynamicMaterial = TileMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void ASAResultTileActor::UpdateVisual()
{
	EnsureDynamicMaterial();

	if (DynamicMaterial)
	{
		const FLinearColor TargetColor = GetColorForCurrentState();

		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("Tint"), TargetColor);
	}
}
