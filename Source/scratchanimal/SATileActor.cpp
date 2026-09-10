// Fill out your copyright notice in the Description page of Project Settings.

#include "SATileActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "SAActorManager.h"
#include "SATileManager.h"

ASATileActor::ASATileActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 기본 속성 초기화
	TileType = ESATileType::Normal;
	WaypointIndex = 1;
	bMoveCameraOnArrival = false;
	CameraFocusCoord = FIntPoint::ZeroValue;
	GridCoord = FIntPoint::ZeroValue;
	bIsSelected = false;
	bIsTrapTile = false;
	bIsMonsterOccupied = false;
	InitialTrapState = ESATileTrapState::Normal;
	InitialDelay = 0.0f;
	NormalDuration = 1.0f;
	TrapDuration = 1.0f;
	CurrentTrapState = ESATileTrapState::Normal;

	// 색상 기본값 설정 (드래그 시 빨간색)
	SelectedColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f);     // 옅은 빨간색
	NormalColor = FLinearColor(0.4f, 0.85f, 0.4f, 1.0f);       // 일반 타일 (옅은 초록색)
	StartColor = FLinearColor(0.1f, 0.85f, 0.2f, 1.0f);          // 시작 타일 (초록색)
	WallColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);        // 벽 타일 (어두운 회색)
	WaypointColor = FLinearColor(0.1f, 0.55f, 1.0f, 1.0f);       // 웨이포인트 타일 (파란색)
	TrapColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);        // 함정 타일 (빨간색)
	GoalColor = FLinearColor(0.01f, 0.01f, 1.f, 1.0f);          // 목표 타일 (파란색)
	MonsterOverlayColor = FLinearColor(0.25f, 0.0f, 0.35f, 1.0f); // 몬스터 점유 (진한 보라색)

	// 웨이포인트 텍스트 기본값 설정
	WaypointTextRotation = FRotator(90.0f, 0.0f, -90.0f);
	WaypointTextColor = FColor::Black;
	WaypointTextWorldSize = 48.0f;

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

	// 3. 웨이포인트 순번 텍스트 컴포넌트 생성 및 설정
	WaypointText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WaypointText"));
	WaypointText->SetupAttachment(RootComponent);
	WaypointText->SetRelativeLocation(FVector(0.0f, 0.0f, 11.5f)); // 타일 상단면(Z=10) 바로 위
	WaypointText->SetRelativeRotation(WaypointTextRotation);
	WaypointText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	WaypointText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	WaypointText->SetWorldSize(WaypointTextWorldSize);
	WaypointText->SetTextRenderColor(WaypointTextColor);
	WaypointText->SetVisibility(false);
}

void ASATileActor::BeginPlay()
{
	Super::BeginPlay();

	EnsureDynamicMaterial();

	// 레벨 배치 시점(에디터)에 지정된 타입이 함정이라면 깜빡임 시작
	bIsTrapTile = (TileType == ESATileType::Trap);
	UpdateVisual();

	if (bIsTrapTile)
	{
		StartTrapBlinkTimer();
	}
}

#if WITH_EDITOR
void ASATileActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	EnsureDynamicMaterial();
	UpdateVisual();
}
#endif

void ASATileActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASATileActor::SetTileType(ESATileType NewType)
{
	TileType = NewType;

	// 함정 타일로 지정되면 깜빡임을 시작하고, 그 외 타입으로 바뀌면 깜빡임을 정지
	bIsTrapTile = (NewType == ESATileType::Trap);
	if (bIsTrapTile)
	{
		StartTrapBlinkTimer();
	}
	else
	{
		StopTrapBlinkTimer();
	}

	UpdateVisual();
}

void ASATileActor::SetTrapCycleConfig(ESATileTrapState InInitialState, float InNormalDuration, float InTrapDuration)
{
	InitialTrapState = InInitialState;
	NormalDuration = FMath::Max(KINDA_SMALL_NUMBER, InNormalDuration);
	TrapDuration = FMath::Max(KINDA_SMALL_NUMBER, InTrapDuration);
}

void ASATileActor::SetTrapInitialDelay(float InInitialDelay)
{
	InitialDelay = FMath::Max(0.0f, InInitialDelay);
}

void ASATileActor::SetGridCoord(int32 InX, int32 InY)
{
	GridCoord = FIntPoint(InX, InY);
}

void ASATileActor::SetWaypointIndex(int32 InIndex)
{
	WaypointIndex = InIndex;
	UpdateVisual();
}

void ASATileActor::SetMoveCameraOnArrival(bool bInMoveCamera)
{
	bMoveCameraOnArrival = bInMoveCamera;
}

void ASATileActor::SetCameraFocusCoord(const FIntPoint& InCoord)
{
	CameraFocusCoord = InCoord;
}

void ASATileActor::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		bIsSelected = bInSelected;
		UpdateVisual();

		// 함정 타일: 선택되어 있는 동안에도 내부 상태 전환 타이머는 절대 멈추거나 일시정지하지 않는다 - 계속 정확히 흘러가되
		// ApplyTrapVisualState()가 bIsSelected를 보고 화면 갱신만 건너뛴다. 드래그 취소 등으로 선택이 해제되는 순간,
		// 다음 타이머 발동을 기다리지 않고 현재 CurrentTrapState를 즉시 화면에 맞춰준다.
		if (bIsTrapTile && !bIsSelected)
		{
			ApplyTrapVisualState();
		}
	}
}

void ASATileActor::SetMonsterOccupied(bool bInOccupied)
{
	if (bIsMonsterOccupied != bInOccupied)
	{
		bIsMonsterOccupied = bInOccupied;
		UpdateVisual();
	}
}

FLinearColor ASATileActor::GetColorForTileType(ESATileType InType) const
{
	switch (InType)
	{
	case ESATileType::Start:
		return StartColor;
	case ESATileType::Wall:
		return WallColor;
	case ESATileType::Waypoint:
		return WaypointColor;
	case ESATileType::Trap:
		return TrapColor;
	case ESATileType::Goal:
		return GoalColor;
	case ESATileType::Normal:
	default:
		return NormalColor;
	}
}

void ASATileActor::EnsureDynamicMaterial()
{
	if (!DynamicMaterial && TileMesh)
	{
		DynamicMaterial = TileMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void ASATileActor::StartTrapBlinkTimer()
{
	if (!bIsTrapTile)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.TimerExists(TrapStateTimerHandle))
	{
		// 이미 돌고 있는 사이클은 다시 손대지 않는다
		return;
	}

	if (InitialDelay > 0.0f)
	{
		// 대기 시간 동안은 항상 Normal(안전) 상태로 표시되고, 대기가 끝나는 순간 InitialTrapState부터 정식 사이클이 시작된다
		CurrentTrapState = ESATileTrapState::Normal;
		ApplyTrapVisualState();
		TimerManager.SetTimer(TrapStateTimerHandle, this, &ASATileActor::BeginTrapCycleAfterDelay, InitialDelay, false);
	}
	else
	{
		// 지정된 초기 상태에서 사이클을 시작하고, 그 상태를 즉시 화면에 반영
		CurrentTrapState = InitialTrapState;
		ApplyTrapVisualState();

		const float FirstDuration = (CurrentTrapState == ESATileTrapState::Normal) ? NormalDuration : TrapDuration;
		TimerManager.SetTimer(TrapStateTimerHandle, this, &ASATileActor::ToggleTrapState, FMath::Max(KINDA_SMALL_NUMBER, FirstDuration), false);
	}
}

void ASATileActor::StopTrapBlinkTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TrapStateTimerHandle);
	}
}

void ASATileActor::BeginTrapCycleAfterDelay()
{
	if (!bIsTrapTile)
	{
		return;
	}

	// 대기 시간이 끝나는 순간, 비로소 InitialTrapState부터 정식 사이클이 시작된다
	CurrentTrapState = InitialTrapState;
	ApplyTrapVisualState();

	if (UWorld* World = GetWorld())
	{
		const float FirstDuration = (CurrentTrapState == ESATileTrapState::Normal) ? NormalDuration : TrapDuration;
		World->GetTimerManager().SetTimer(TrapStateTimerHandle, this, &ASATileActor::ToggleTrapState, FMath::Max(KINDA_SMALL_NUMBER, FirstDuration), false);
	}
}

void ASATileActor::ToggleTrapState()
{
	if (!bIsTrapTile)
	{
		return;
	}

	// 상태를 뒤바꾸고, 새 상태의 유지 시간이 지나면 다시 이 함수가 불리도록 예약한다.
	// 선택 중이라도 이 전환과 재예약 자체는 멈추지 않아야 다음 전환 시점이 어긋나지 않는다.
	CurrentTrapState = (CurrentTrapState == ESATileTrapState::Normal) ? ESATileTrapState::Trap : ESATileTrapState::Normal;
	ApplyTrapVisualState();

	if (UWorld* World = GetWorld())
	{
		const float NextDuration = (CurrentTrapState == ESATileTrapState::Normal) ? NormalDuration : TrapDuration;
		World->GetTimerManager().SetTimer(TrapStateTimerHandle, this, &ASATileActor::ToggleTrapState, FMath::Max(KINDA_SMALL_NUMBER, NextDuration), false);
	}
}

void ASATileActor::ApplyTrapVisualState()
{
	if (!bIsTrapTile)
	{
		return;
	}

	const ESATileType NewVisualType = (CurrentTrapState == ESATileTrapState::Trap) ? ESATileType::Trap : ESATileType::Normal;

	// 드래그 경로에 포함되어(선택되어) 있는 동안 Trap으로 전환되면, 화면 갱신은 미루되
	// "밟고 있던 칸이 함정이 됐다"는 사실만은 즉시 매니저에 알려 지금 함정을 밟은 것과 동일하게 처리한다.
	// (경로가 초기화되면 SetSelected(false)가 호출되어, 그때 아래 분기에서 화면도 정상적으로 갱신된다.)
	if (bIsSelected)
	{
		if (NewVisualType == ESATileType::Trap)
		{
			if (USATileManager* TileManager = USATileManager::Get(this))
			{
				TileManager->NotifyTileBecameTrapWhileHeld(this);
			}
		}
		return;
	}

	if (TileType != NewVisualType)
	{
		TileType = NewVisualType;
		UpdateVisual();

		// Trap 상태가 되면 이 타일 위에 트랩 이펙트를 켜고, Normal로 돌아가면 꺼서 풀에 반납
		if (USAActorManager* ActorManager = USAActorManager::Get(this))
		{
			if (NewVisualType == ESATileType::Trap)
			{
				ActorManager->ShowTrapEffect(this);
			}
			else
			{
				ActorManager->HideTrapEffect(this);
			}
		}
	}
}

void ASATileActor::UpdateVisual()
{
	EnsureDynamicMaterial();

	if (DynamicMaterial)
	{
		// 몬스터가 점유 중이면 보라색이 최우선. 웨이포인트/시작 타일이 선택되면 고유 색상이 완전히 덮이지
		// 않도록 SelectedColor와 절반씩 섞은 혼합색을 쓰고, 그 외 타입은 선택 시 SelectedColor(빨간색)를
		// 그대로, 선택되지 않았으면 타일 타입에 따른 색상을 적용한다.
		FLinearColor TargetColor;
		if (bIsMonsterOccupied)
		{
			TargetColor = MonsterOverlayColor;
		}
		else if (bIsSelected && TileType == ESATileType::Waypoint)
		{
			TargetColor = FMath::Lerp(WaypointColor, SelectedColor, 0.5f);
		}
		else if (bIsSelected && TileType == ESATileType::Start)
		{
			TargetColor = FMath::Lerp(StartColor, SelectedColor, 0.5f);
		}
		else if (bIsSelected)
		{
			TargetColor = SelectedColor;
		}
		else
		{
			TargetColor = GetColorForTileType(TileType);
		}

		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), TargetColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("Tint"), TargetColor);
	}

	// 웨이포인트 타일일 경우 순번(1, 2, 3...) 텍스트 렌더링
	if (WaypointText)
	{
		if (TileType == ESATileType::Waypoint && WaypointIndex > 0)
		{
			WaypointText->SetText(FText::AsNumber(WaypointIndex));
			WaypointText->SetTextRenderColor(WaypointTextColor);
			WaypointText->SetWorldSize(WaypointTextWorldSize);
			WaypointText->SetRelativeRotation(WaypointTextRotation);
			WaypointText->SetVisibility(true);
		}
		else
		{
			WaypointText->SetVisibility(false);
		}
	}
}
