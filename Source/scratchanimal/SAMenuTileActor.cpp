// Fill out your copyright notice in the Description page of Project Settings.

#include "SAMenuTileActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ASAMenuTileActor::ASAMenuTileActor()
{
	PrimaryActorTick.bCanEverTick = false;

	GridCoord = FIntPoint::ZeroValue;
	bIsStartTile = false;
	AssignedLevelNumber = 0;
	bIsSelected = false;

	NormalColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);   // 빈 타일 (밝은 회색) - 레벨 그리드의 무지개 파스텔 그룹 색상이 눈에 띄도록 중립색 유지
	StartColor = FLinearColor(0.1f, 0.85f, 0.2f, 1.0f);      // 시작 타일 (SATileActor의 Start와 동일한 초록색)
	LevelColor = FLinearColor(0.1f, 0.55f, 1.0f, 1.0f);      // 레벨 타일 (파란색)
	SelectedColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f);    // 선택된 타일 (SATileActor의 Selected와 동일한 빨간색)

	LevelTextRotation = FRotator(90.0f, 0.0f, -90.0f);
	LevelTextColor = FColor::Black;
	LevelTextWorldSize = 48.0f;

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

	// 3. 레벨 번호 텍스트 컴포넌트 생성 및 설정
	LevelNumberText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LevelNumberText"));
	LevelNumberText->SetupAttachment(RootComponent);
	LevelNumberText->SetRelativeLocation(FVector(0.0f, 0.0f, 11.5f)); // 타일 상단면(Z=10) 바로 위
	LevelNumberText->SetRelativeRotation(LevelTextRotation);
	LevelNumberText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	LevelNumberText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	LevelNumberText->SetWorldSize(LevelTextWorldSize);
	LevelNumberText->SetTextRenderColor(LevelTextColor);
	LevelNumberText->SetVisibility(false);
}

void ASAMenuTileActor::BeginPlay()
{
	Super::BeginPlay();

	EnsureDynamicMaterial();
	UpdateVisual();
}

#if WITH_EDITOR
void ASAMenuTileActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	EnsureDynamicMaterial();
	UpdateVisual();
}
#endif

void ASAMenuTileActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASAMenuTileActor::SetGridCoord(int32 InX, int32 InY)
{
	GridCoord = FIntPoint(InX, InY);
}

void ASAMenuTileActor::SetIsStartTile(bool bInIsStartTile)
{
	bIsStartTile = bInIsStartTile;
	UpdateVisual();
}

void ASAMenuTileActor::SetAssignedLevelNumber(int32 InLevelNumber)
{
	AssignedLevelNumber = InLevelNumber;
	UpdateVisual();
}

void ASAMenuTileActor::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		bIsSelected = bInSelected;
		UpdateVisual();
	}
}

FLinearColor ASAMenuTileActor::GetColorForCurrentState() const
{
	if (bIsSelected)
	{
		return SelectedColor;
	}

	if (bIsStartTile)
	{
		return StartColor;
	}

	if (AssignedLevelNumber > 0)
	{
		return LevelColor;
	}

	return NormalColor;
}

void ASAMenuTileActor::EnsureDynamicMaterial()
{
	if (!DynamicMaterial && TileMesh)
	{
		DynamicMaterial = TileMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void ASAMenuTileActor::UpdateVisual()
{
	EnsureDynamicMaterial();

	if (DynamicMaterial)
	{
		const FLinearColor TargetColor = GetColorForCurrentState();

		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("Tint"), TargetColor);
	}

	// 등록된 레벨이 있는 타일만 번호 텍스트를 표시
	if (LevelNumberText)
	{
		if (AssignedLevelNumber > 0)
		{
			LevelNumberText->SetText(FText::AsNumber(AssignedLevelNumber));
			LevelNumberText->SetTextRenderColor(LevelTextColor);
			LevelNumberText->SetWorldSize(LevelTextWorldSize);
			LevelNumberText->SetRelativeRotation(LevelTextRotation);
			LevelNumberText->SetVisibility(true);
		}
		else
		{
			LevelNumberText->SetVisibility(false);
		}
	}
}
