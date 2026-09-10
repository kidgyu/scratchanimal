// Fill out your copyright notice in the Description page of Project Settings.

#include "SAPlayerPawn.h"
#include "SATileManager.h"
#include "SATileSettings.h"
#include "SAGameState.h"
#include "SAMenuManager.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASAPlayerPawn::ASAPlayerPawn()
{
	// 카메라 이동 웨이포인트 Lerp 이동(레벨 16~20용)을 Tick에서 처리하기 위해 활성화
	PrimaryActorTick.bCanEverTick = true;

	// Create Root Scene Component
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 기본 설정: 완전 탑다운 뷰 (-89.9도, Yaw -90도: 화면 오른쪽이 +X, 아래쪽이 +Y)
	CameraRotation = FRotator(-70.0f, -90.0f, 0.0f);
	CameraPaddingRatio = 1.05f; // 화면에 꽉 차도록 5% 여유
	ScreenMarginRatio = 0.04f;  // 화면 상하좌우 4% 안전 여백
	MinCameraDistance = 300.0f; // 작은 그리드도 화면에 꽉 차도록 최소 거리 완화
	bAutoFocusOnLevelLoaded = true;
	ResultScreenReferenceLevel = 10;
	MaxCameraDistanceReferenceLevel = 15; // 10x10 레벨(15, 21~24 등)까지 잘리지 않도록 기준을 8x8(레벨10)에서 10x10(레벨15)으로 확대
	WaypointCameraMoveDuration = 0.6f;
	MenuButtonScreenMarginRatio = FVector2D(0.09f, 0.07f); // 높이를 띄운 만큼 화면 밖으로 잘리지 않도록 왼쪽 아래로 더 들여서 배치

	bIsPanningToWaypoint = false;
	WaypointPanStartLocation = FVector::ZeroVector;
	WaypointPanTargetLocation = FVector::ZeroVector;
	WaypointPanElapsedTime = 0.0f;

	// Create SpringArm Component
	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComp"));
	SpringArmComp->SetupAttachment(RootComponent);
	SpringArmComp->SetRelativeRotation(CameraRotation);
	SpringArmComp->TargetArmLength = 1200.0f;
	SpringArmComp->bDoCollisionTest = false;
	SpringArmComp->bInheritPitch = false;
	SpringArmComp->bInheritYaw = false;
	SpringArmComp->bInheritRoll = false;
	SpringArmComp->bEnableCameraLag = false;

	// Create Camera Component and attach to the SpringArm socket
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);
	CameraComp->bUsePawnControlRotation = false;
	CameraComp->FieldOfView = 90.0f;
}

void ASAPlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	// 타일 매니저 이벤트에 바인딩
	if (USATileManager* TileManager = USATileManager::Get(this))
	{
		TileManager->OnLevelLoaded.RemoveDynamic(this, &ASAPlayerPawn::OnLevelLoadedHandler);
		TileManager->OnLevelLoaded.AddDynamic(this, &ASAPlayerPawn::OnLevelLoadedHandler);

		TileManager->OnWaypointCameraMoveRequested.RemoveDynamic(this, &ASAPlayerPawn::OnWaypointCameraMoveRequestedHandler);
		TileManager->OnWaypointCameraMoveRequested.AddDynamic(this, &ASAPlayerPawn::OnWaypointCameraMoveRequestedHandler);

		// 이미 레벨이 로드된 상태라면 즉시 포커스
		if (TileManager->CurrentLevelNumber > 0)
		{
			FocusOnTileGrid();
		}
	}

	// GameState의 상태 전환 이벤트에 바인딩 (Result 상태가 되면 FocusOnResultScreen 호출)
	if (ASAGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ASAGameState>() : nullptr)
	{
		GameState->OnGameStateChanged.RemoveDynamic(this, &ASAPlayerPawn::OnGameStateChangedHandler);
		GameState->OnGameStateChanged.AddDynamic(this, &ASAPlayerPawn::OnGameStateChangedHandler);
	}

	// BeginPlay 직후 안전하게 그리드 중심을 잡도록 1프레임 딜레이 타이머도 등록
	FTimerHandle FocusTimerHandle;
	GetWorldTimerManager().SetTimer(FocusTimerHandle, this, &ASAPlayerPawn::FocusOnTileGrid, 0.05f, false);
}

void ASAPlayerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsPanningToWaypoint)
	{
		return;
	}

	WaypointPanElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(WaypointPanElapsedTime / FMath::Max(WaypointCameraMoveDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	SetActorLocation(FMath::Lerp(WaypointPanStartLocation, WaypointPanTargetLocation, Alpha));

	if (Alpha >= 1.0f)
	{
		bIsPanningToWaypoint = false;

		// 카메라 이동이 끝났으니 SATileManager에 알려 잠갔던 터치 입력을 다시 풀어준다
		if (USATileManager* TileManager = USATileManager::Get(this))
		{
			TileManager->NotifyCameraMoveCompleted();
		}
	}
}

void ASAPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ASAPlayerPawn::OnLevelLoadedHandler(int32 LevelNumber)
{
	if (bAutoFocusOnLevelLoaded)
	{
		FocusOnTileGrid();
	}
}

void ASAPlayerPawn::OnGameStateChangedHandler(ESAGameStateType NewState, ESAGameStateType PrevState)
{
	if (NewState == ESAGameStateType::Result)
	{
		FocusOnResultScreen();
	}
	else if (NewState == ESAGameStateType::Menu)
	{
		FocusOnMenu();
	}
}

void ASAPlayerPawn::OnWaypointCameraMoveRequestedHandler(FVector TargetLocation)
{
	bIsPanningToWaypoint = true;
	WaypointPanStartLocation = GetActorLocation();
	WaypointPanTargetLocation = TargetLocation;
	WaypointPanElapsedTime = 0.0f;
}

void ASAPlayerPawn::FocusOnTileGrid()
{
	USATileManager* TileManager = USATileManager::Get(this);
	if (!TileManager)
	{
		return;
	}

	FocusCameraOnGrid(TileManager->GetGridCenterLocation(), TileManager->CurrentLevelData.GridWidth, TileManager->CurrentLevelData.GridHeight, TileManager->TileStep);

	// 화면 우상단 나가기 슬라이더는 카메라 거리가 방금 확정된 뒤에 배치해야 정확한 화면 위치가 나온다.
	// Result 화면 포커스(FocusOnResultScreen)에서는 필요 없으므로, 실제로 Game 상태일 때만 (재)배치한다.
	// 단, 이 함수 안에서 SpringArm 거리를 바꾼 바로 이번 프레임에는 실제 렌더링에 쓰이는 카메라
	// 트랜스폼(DeprojectScreenPositionToWorld가 참조)이 아직 새 거리로 갱신되기 전이라, 곧바로 배치하면
	// (특히 게임을 막 시작한 첫 레벨에서) 위치가 어긋날 수 있다. 한 프레임 정도 늦춰서 배치한다.
	if (ASAGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ASAGameState>() : nullptr)
	{
		if (GameState->GetCurrentStateType() == ESAGameStateType::Game)
		{
			FTimerHandle MenuButtonSpawnTimerHandle;
			GetWorldTimerManager().SetTimer(MenuButtonSpawnTimerHandle, TileManager, &USATileManager::SpawnMenuButtonTiles, 0.05f, false);
		}
	}
}

void ASAPlayerPawn::FocusOnResultScreen()
{
	// Result 화면은 자체 타일 그리드가 없으므로, ResultScreenReferenceLevel(기본 10)을 열었을 때와
	// 동일한 카메라 위치/거리가 되도록 그 레벨의 그리드 크기를 그대로 재사용한다.
	const USATileSettings* Settings = USATileSettings::Get();
	FSATileLevelData ReferenceLevelData;
	if (!Settings || !Settings->GetLevelData(ResultScreenReferenceLevel, ReferenceLevelData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ASAPlayerPawn] Result 화면 카메라 기준 레벨 %d 데이터를 찾을 수 없습니다."), ResultScreenReferenceLevel);
		return;
	}

	USATileManager* TileManager = USATileManager::Get(this);
	const float TileStep = TileManager ? TileManager->TileStep : 105.0f;

	FocusCameraOnGrid(FVector::ZeroVector, ReferenceLevelData.GridWidth, ReferenceLevelData.GridHeight, TileStep);
}

void ASAPlayerPawn::FocusOnMenu()
{
	USAMenuManager* MenuManager = USAMenuManager::Get(this);
	if (!MenuManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ASAPlayerPawn] SAMenuManager 서브시스템을 찾을 수 없어 메뉴 카메라를 되돌릴 수 없습니다."));
		return;
	}

	// 메뉴 그리드는 항상 GridOrigin을 중심으로 고정 배치되므로(bCenterGrid), 인게임 중 카메라 이동
	// 웨이포인트로 폰이 아무리 멀리 이동해 있었더라도 메뉴 그리드 크기 기준으로 다시 맞춰준다.
	FocusCameraOnGrid(MenuManager->GridOrigin, MenuManager->GridWidth, MenuManager->GridHeight, MenuManager->TileStep);
}

bool ASAPlayerPawn::GetTopRightCornerOffset(float InHeightOffset, FVector& OutOffset) const
{
	// 근사 삼각함수 계산 대신, 실제 화면 좌표를 카메라 투영 그대로 역투영(deproject)해서 정확한
	// 월드 위치를 구한다. 카메라가 완전 수직(-90도)이 아니라 기울어져 있어(CameraRotation),
	// 단순 FOV*거리 근사로는 화면 모서리와 정확히 일치하지 않고 타일과 겹치는 오차가 생길 수 있다.
	//
	// 실패 시 OutOffset을 (0,0,InHeightOffset)으로 두지 않는다 - 그러면 폰(그리드 중심) 바로 위에
	// 스폰돼 인게임 타일과 겹쳐 보이는 문제가 생긴다. 대신 false를 반환해서 호출 측이 재시도하게 한다.
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	}

	if (!PC)
	{
		return false;
	}

	// 레벨 로드 직후 등 뷰포트가 아직 제대로 초기화되지 않은 시점에는 GetViewportSize가 0을 반환할 수 있다
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PC->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return false;
	}

	// 뷰포트 우상단 모서리에서 MenuButtonScreenMarginRatio만큼만 안쪽으로 들인 실제 화면 좌표
	// (화면 좌표계는 좌상단이 (0,0), 우하단이 (Width,Height))
	const float ScreenX = ViewportWidth * (1.0f - MenuButtonScreenMarginRatio.X);
	const float ScreenY = ViewportHeight * MenuButtonScreenMarginRatio.Y;

	FVector RayOrigin, RayDirection;
	if (!PC->DeprojectScreenPositionToWorld(ScreenX, ScreenY, RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Z))
	{
		return false;
	}

	// 폰 위치보다 InHeightOffset만큼 높은 수평면과 화면 좌표에서 뻗어나간 레이의 교차점을 구한다
	const float PlaneZ = GetActorLocation().Z + InHeightOffset;
	const float T = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
	const FVector WorldPoint = RayOrigin + RayDirection * T;

	OutOffset = WorldPoint - GetActorLocation();
	return true;
}

void ASAPlayerPawn::FocusCameraOnGrid(const FVector& InCenter, int32 InGridWidth, int32 InGridHeight, float InTileStep)
{
	const int32 GridW = FMath::Max(1, InGridWidth);
	const int32 GridH = FMath::Max(1, InGridHeight);
	constexpr float TileVisualSize = 100.0f;

	// 실제 타일 그리드가 차지하는 정확한 외곽 월드 크기 (첫 타일 왼쪽 끝 ~ 마지막 타일 오른쪽 끝)
	const float WorldSpanX = (GridW - 1) * InTileStep + TileVisualSize;
	const float WorldSpanY = (GridH - 1) * InTileStep + TileVisualSize;

	// 1. 폰의 위치를 그리드의 중심점 위치로 즉시 이동
	SetActorLocation(InCenter);

	// 2. 스프링암 회전 적용
	if (SpringArmComp)
	{
		SpringArmComp->SetRelativeRotation(CameraRotation);

		// 3. 뷰포트 종횡비(가로/세로 비율) 확인
		float AspectRatio = 16.0f / 9.0f;
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC)
		{
			PC = GetWorld()->GetFirstPlayerController();
		}

		if (PC)
		{
			int32 ViewportWidth = 0;
			int32 ViewportHeight = 0;
			PC->GetViewportSize(ViewportWidth, ViewportHeight);
			if (ViewportWidth > 0 && ViewportHeight > 0)
			{
				AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
			}
		}

		// 4. 수평 및 수직 FOV를 모두 반영한 정밀 거리 계산
		const float FOVdeg = CameraComp ? CameraComp->FieldOfView : 90.0f;
		const float HalfFOVxRad = FMath::DegreesToRadians(FOVdeg * 0.5f);
		const float TanHalfFOVx = FMath::Tan(HalfFOVxRad);
		// 세로 FOV 탄젠트: tan(FOVy/2) = tan(FOVx/2) / AspectRatio
		const float TanHalfFOVy = TanHalfFOVx / AspectRatio;

		// 화면 테두리 안전 여백(ScreenMarginRatio: 기본 0.04 = 상하좌우 4% 여백)
		// 유효 안전 화면 비율: 1.0 - 2 * ScreenMarginRatio (예: 92%)
		const float SafeViewportRatio = FMath::Clamp(1.0f - (ScreenMarginRatio * 2.0f), 0.5f, 1.0f);

		// 가로(X)를 화면 안전 영역 안에 모두 담기 위한 카메라 거리
		const float DistForWidth = (WorldSpanX * 0.5f) / (TanHalfFOVx * SafeViewportRatio);
		// 세로(Y)를 화면 안전 영역 안에 모두 담기 위한 카메라 거리
		const float DistForHeight = (WorldSpanY * 0.5f) / (TanHalfFOVy * SafeViewportRatio);

		// 가로와 세로 중 화면에 먼저 닿는 축을 기준으로 카메라 거리 확정
		float DesiredDistance = FMath::Max(DistForWidth, DistForHeight);
		DesiredDistance = FMath::Max(MinCameraDistance, DesiredDistance);

		// MaxCameraDistanceReferenceLevel(기본 15)의 그리드 크기가 요구하는 거리를 넘지 않도록 상한을 둔다.
		// 그보다 그리드가 큰 레벨(16~20의 카메라 이동 기믹 등)에서도 카메라가 더 줌아웃되지 않고,
		// 대신 카메라가 웨이포인트를 따라 패닝하는 방식으로 넓은 그리드를 보여준다.
		if (const USATileSettings* Settings = USATileSettings::Get())
		{
			FSATileLevelData MaxDistanceLevelData;
			if (Settings->GetLevelData(MaxCameraDistanceReferenceLevel, MaxDistanceLevelData))
			{
				const int32 MaxGridW = FMath::Max(1, MaxDistanceLevelData.GridWidth);
				const int32 MaxGridH = FMath::Max(1, MaxDistanceLevelData.GridHeight);
				const float MaxWorldSpanX = (MaxGridW - 1) * InTileStep + TileVisualSize;
				const float MaxWorldSpanY = (MaxGridH - 1) * InTileStep + TileVisualSize;
				const float MaxDistForWidth = (MaxWorldSpanX * 0.5f) / (TanHalfFOVx * SafeViewportRatio);
				const float MaxDistForHeight = (MaxWorldSpanY * 0.5f) / (TanHalfFOVy * SafeViewportRatio);
				const float MaxCameraDistance = FMath::Max(MaxDistForWidth, MaxDistForHeight);

				DesiredDistance = FMath::Min(DesiredDistance, MaxCameraDistance);
			}
		}

		SpringArmComp->TargetArmLength = DesiredDistance;

		UE_LOG(LogTemp, Display, TEXT("[ASAPlayerPawn] 카메라 포커스 완료! 그리드 %dx%d (%s), 카메라 거리: %.1f (종횡비: %.2f)"),
			GridW, GridH, *InCenter.ToString(), DesiredDistance, AspectRatio);
	}
}

void ASAPlayerPawn::CheckAndAdjustCameraBounds()
{
	// FocusOnTileGrid()에서 삼각함수 기반 단일 패스로 완벽한 거리를 즉시 산출하므로,
	// 화면이 덜컹거리거나(작았다가 커지는 현상) 왜곡되는 것을 방지하기 위해 중복 변경을 수행하지 않습니다.
}
