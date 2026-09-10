// Fill out your copyright notice in the Description page of Project Settings.

#include "SATileManager.h"
#include "SATileSettings.h"
#include "SASoundManager.h"
#include "SAPlayerPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

USATileManager::USATileManager()
{
	TileActorClass = ASATileActor::StaticClass();
	TileStep = 105.0f; // 타일 크기 100 + 5 간격
	GridOrigin = FVector::ZeroVector;
	bCenterGrid = true;
	CurrentLevelNumber = 0;
	StartTile = nullptr;
	GoalTile = nullptr;

	NextWaypointIndexToVisit = 1;
	SavedWaypointIndex = 1;
	bIsDragging = false;
	bLevelCompleted = false;

	bInputLocked = false;
	WipeDuration = 1.2f;
	WipeStepInterval = 0.05f;
	WipeIndex = 0;
	WipeTilesPerStep = 1;

	MenuButtonHeightOffset = 30.0f; // 기울어진 카메라 시점에서 타일과 겹쳐 보이지 않도록 그리드 표면보다 띄움
	MenuButtonTileGap = TileStep;
	MenuButtonColor = FLinearColor(0.05f, 0.1f, 0.5f, 1.0f); // 진한 파란색
	bIsMenuButtonDragging = false;
}

void USATileManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USATileManager::Deinitialize()
{
	ClearTiles();
	Super::Deinitialize();
}

USATileManager* USATileManager::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<USATileManager>() : nullptr;
}

bool USATileManager::LoadLevel(int32 InLevelNumber)
{
	const USATileSettings* Settings = USATileSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("[USATileManager] SATileSettings를 찾을 수 없습니다."));
		return false;
	}

	FSATileLevelData LevelData;
	if (!Settings->GetLevelData(InLevelNumber, LevelData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[USATileManager] 레벨 %d의 데이터를 찾을 수 없습니다."), InLevelNumber);
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[USATileManager] 유효한 World가 없습니다."));
		return false;
	}

	// 기존 타일 정리
	ClearTiles();

	CurrentLevelNumber = InLevelNumber;
	CurrentLevelData = LevelData;

	// 경로 진행 상태 초기화
	SelectedPath.Empty();
	SavedCheckpointPath.Empty();
	NextWaypointIndexToVisit = 1;
	SavedWaypointIndex = 1;
	bIsDragging = false;
	bLevelCompleted = false;
	bInputLocked = false;

	UClass* ClassToSpawn = TileActorClass ? TileActorClass.Get() : ASATileActor::StaticClass();

	// 전체 그리드에 타일 액터 스폰
	for (int32 Y = 0; Y < CurrentLevelData.GridHeight; ++Y)
	{
		for (int32 X = 0; X < CurrentLevelData.GridWidth; ++X)
		{
			const FIntPoint Coord(X, Y);
			const FVector SpawnLoc = CalculateTileWorldLocation(X, Y);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			ASATileActor* NewTile = World->SpawnActor<ASATileActor>(ClassToSpawn, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
			if (NewTile)
			{
				int32 WaypointIdx = 0;
				const ESATileType TileType = CurrentLevelData.GetTileTypeAt(Coord, WaypointIdx);

				NewTile->SetGridCoord(X, Y);
				NewTile->SetWaypointIndex(WaypointIdx);

				// 함정 타일이면 SetTileType으로 사이클 타이머가 최초 생성되기 전에 레벨별 오버라이드를 먼저 반영
				if (TileType == ESATileType::Trap)
				{
					ESATileTrapState OverrideInitialState;
					float OverrideNormalDuration, OverrideTrapDuration;
					if (CurrentLevelData.GetTrapCycleOverride(Coord, OverrideInitialState, OverrideNormalDuration, OverrideTrapDuration))
					{
						NewTile->SetTrapCycleConfig(OverrideInitialState, OverrideNormalDuration, OverrideTrapDuration);
					}

					const float OverrideInitialDelay = CurrentLevelData.GetTrapInitialDelay(Coord);
					if (OverrideInitialDelay > 0.0f)
					{
						NewTile->SetTrapInitialDelay(OverrideInitialDelay);
					}
				}

				NewTile->SetTileType(TileType);

				SpawnedTileMap.Add(Coord, NewTile);

				if (TileType == ESATileType::Start)
				{
					StartTile = NewTile;
				}
				else if (TileType == ESATileType::Goal)
				{
					GoalTile = NewTile;
				}
				else if (TileType == ESATileType::Waypoint)
				{
					WaypointTileMap.Add(WaypointIdx, NewTile);
					NewTile->SetMoveCameraOnArrival(CurrentLevelData.DoesWaypointMoveCamera(Coord));
					NewTile->SetCameraFocusCoord(CurrentLevelData.GetWaypointCameraFocusCoord(Coord));
				}
			}
		}
	}

	// 화면 우상단 나가기 슬라이더는 카메라 거리가 확정된 뒤에 배치해야 하므로, 여기서 직접 스폰하지 않고
	// ASAPlayerPawn::FocusOnTileGrid가 이 브로드캐스트를 받아 카메라 포커스를 마친 직후 스폰을 요청한다.

	UE_LOG(LogTemp, Log, TEXT("[USATileManager] 레벨 %d 로드 완료! (그리드: %dx%d, 총 타일 수: %d, 웨이포인트: %d개)"),
		CurrentLevelNumber, CurrentLevelData.GridWidth, CurrentLevelData.GridHeight, SpawnedTileMap.Num(), CurrentLevelData.WaypointCoords.Num());

	OnLevelLoaded.Broadcast(CurrentLevelNumber);
	return true;
}

bool USATileManager::RestartCurrentLevel()
{
	if (CurrentLevelNumber > 0)
	{
		return LoadLevel(CurrentLevelNumber);
	}
	return false;
}

bool USATileManager::LoadNextLevel()
{
	const USATileSettings* Settings = USATileSettings::Get();
	const int32 TotalLevels = Settings ? Settings->GetTotalLevelCount() : 50;

	if (CurrentLevelNumber >= TotalLevels)
	{
		UE_LOG(LogTemp, Display, TEXT("[USATileManager] 마지막 레벨(%d)입니다. 더 이상 다음 레벨이 없습니다."), TotalLevels);
		return false;
	}

	return LoadLevel(CurrentLevelNumber + 1);
}

void USATileManager::ClearTiles()
{
	DestroyMenuButtonTiles();

	for (auto& Pair : SpawnedTileMap)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}

	SpawnedTileMap.Empty();
	WaypointTileMap.Empty();
	StartTile = nullptr;
	GoalTile = nullptr;

	SelectedPath.Empty();
	SavedCheckpointPath.Empty();
	NextWaypointIndexToVisit = 1;
	SavedWaypointIndex = 1;
	bIsDragging = false;
	bLevelCompleted = false;

	OnTilesCleared.Broadcast();
}

void USATileManager::ResetAllTilesSelection()
{
	for (auto& Pair : SpawnedTileMap)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->SetSelected(false);
		}
	}
}

// -------------------------------------------------------------
// 터치 및 드래그 경로 처리
// -------------------------------------------------------------

bool USATileManager::AreTilesAdjacent(const ASATileActor* TileA, const ASATileActor* TileB)
{
	if (!TileA || !TileB)
	{
		return false;
	}

	const FIntPoint CoordA = TileA->GetGridCoord();
	const FIntPoint CoordB = TileB->GetGridCoord();

	// 상, 하, 좌, 우 4방향 인접 (맨해튼 거리 == 1)
	const int32 ManhattanDist = FMath::Abs(CoordA.X - CoordB.X) + FMath::Abs(CoordA.Y - CoordB.Y);
	return ManhattanDist == 1;
}

void USATileManager::ProcessTileTouchBegin(ASATileActor* TouchedTile)
{
	if (bInputLocked || !TouchedTile)
	{
		return;
	}

	// 화면 우상단 나가기 슬라이더 (레벨 진행/클리어 상태와 무관하게 항상 동작)
	if (TouchedTile == MenuButtonTileTop)
	{
		bIsMenuButtonDragging = true;
		MenuButtonSelectedPath.Reset();
		MenuButtonSelectedPath.Add(TouchedTile);
		TouchedTile->SetSelected(true);

		// 아래쪽 타일을 드러내 아래로 드래그할 수 있게 한다
		if (MenuButtonTileBottom)
		{
			MenuButtonTileBottom->SetActorHiddenInGame(false);
			MenuButtonTileBottom->SetActorEnableCollision(true);
		}
		return;
	}

	if (bLevelCompleted)
	{
		return;
	}

	bIsDragging = true;

	// 아직 경로가 비어있는 경우: 반드시 시작 타일(Start)부터 터치해야 함!
	if (SelectedPath.IsEmpty())
	{
		if (TouchedTile->GetTileType() == ESATileType::Start)
		{
			SelectedPath.Add(TouchedTile);
			TouchedTile->SetSelected(true);
			UE_LOG(LogTemp, Log, TEXT("[USATileManager] 시작 타일 터치: 드래그 시작!"));
		}
		else
		{
			// 시작 지점이 아니면 터치 인식하지 않음
			UE_LOG(LogTemp, Verbose, TEXT("[USATileManager] 시작 타일이 아니므로 터치가 무시되었습니다."));
		}
		return;
	}

	// 이미 체크포인트(웨이포인트)가 저장되어 있는 상태에서 손을 뗐다가 다시 터치한 경우
	ASATileActor* LastTile = SelectedPath.Last();
	if (TouchedTile == LastTile)
	{
		// 마지막으로 저장된 타일을 다시 눌렀으므로 그대로 드래그 이어가기
		return;
	}

	if (AreTilesAdjacent(LastTile, TouchedTile))
	{
		// 저장된 마지막 타일과 인접한 타일을 터치했다면 바로 이동 처리로 연결
		ProcessTileTouchMove(TouchedTile);
		return;
	}

	// 시작 타일을 다시 누르면 처음부터 다시 시작
	if (TouchedTile->GetTileType() == ESATileType::Start)
	{
		ResetDragPath(true);
		SelectedPath.Add(TouchedTile);
		TouchedTile->SetSelected(true);
		UE_LOG(LogTemp, Log, TEXT("[USATileManager] 시작 타일 재터치: 경로 초기화 후 다시 시작!"));
		return;
	}

	// 이미 지나온(선택된) 웨이포인트 타일을 다시 누르면, 그 이후에 선택했던 타일들은 모두 해제하고
	// 해당 웨이포인트 지점까지 경로를 되돌린다 (시작 타일 재터치와 동일한 개념의 부분 되돌리기)
	if (TouchedTile->GetTileType() == ESATileType::Waypoint)
	{
		const int32 FoundIdx = SelectedPath.IndexOfByKey(TouchedTile);
		if (FoundIdx != INDEX_NONE)
		{
			for (int32 i = SelectedPath.Num() - 1; i > FoundIdx; --i)
			{
				if (IsValid(SelectedPath[i]))
				{
					SelectedPath[i]->SetSelected(false);
				}
			}
			SelectedPath.SetNum(FoundIdx + 1);

			// 되돌아간 지점을 기준으로 다음 웨이포인트 인덱스와 체크포인트를 다시 맞춘다
			NextWaypointIndexToVisit = TouchedTile->GetWaypointIndex() + 1;
			SavedCheckpointPath = SelectedPath;
			SavedWaypointIndex = NextWaypointIndexToVisit;

			UE_LOG(LogTemp, Log, TEXT("[USATileManager] 이미 선택된 웨이포인트 %d 재터치: 해당 지점으로 경로 롤백!"), TouchedTile->GetWaypointIndex());
			OnPathReset.Broadcast();
		}
	}
}

void USATileManager::ProcessTileTouchMove(ASATileActor* TouchedTile)
{
	if (bInputLocked || !TouchedTile)
	{
		return;
	}

	// 나가기 슬라이더 드래그 중이면 그쪽 로직만 처리하고 레벨 경로 로직은 건드리지 않음
	if (bIsMenuButtonDragging)
	{
		if (TouchedTile == MenuButtonTileBottom && MenuButtonSelectedPath.Num() == 1)
		{
			MenuButtonSelectedPath.Add(TouchedTile);
			TouchedTile->SetSelected(true);

			if (USASoundManager* SoundManager = USASoundManager::Get(this))
			{
				SoundManager->PlaySound2D(ESASFXType::Normal);
			}
		}
		return;
	}

	if (!bIsDragging || bLevelCompleted)
	{
		return;
	}

	// 경로가 비어있을 때는 드래그 중이라도 시작 타일 위로 올라와야 경로가 개시됨
	if (SelectedPath.IsEmpty())
	{
		if (TouchedTile->GetTileType() == ESATileType::Start)
		{
			SelectedPath.Add(TouchedTile);
			TouchedTile->SetSelected(true);
			UE_LOG(LogTemp, Log, TEXT("[USATileManager] 드래그 중 시작 타일 진입!"));
		}
		return;
	}

	ASATileActor* LastTile = SelectedPath.Last();

	// 동일한 타일 위라면 변화 없음
	if (TouchedTile == LastTile)
	{
		return;
	}

	// 1. 직전 타일로 되돌아온 경우 (Undo / 되돌리기 기능)
	if (SelectedPath.Num() >= 2 && TouchedTile == SelectedPath[SelectedPath.Num() - 2])
	{
		// 단, 이미 체크포인트(웨이포인트)로 확정 저장된 타일은 되돌릴 수 없음
		if (!SavedCheckpointPath.Contains(LastTile))
		{
			LastTile->SetSelected(false);
			SelectedPath.Pop();
			UE_LOG(LogTemp, Verbose, TEXT("[USATileManager] 직전 타일로 되돌아와 취소 처리됨"));
			return;
		}
	}

	// 2. 인접성 검사: "이전 터치 영역과 인접하지 않으면 터치 인식이 안되게"
	if (!AreTilesAdjacent(LastTile, TouchedTile))
	{
		// 대각선이나 먼 거리 점프는 인식하지 않음
		return;
	}

	// 3. 이미 현재 경로에 포함된 타일은 재진입 불가
	if (SelectedPath.Contains(TouchedTile))
	{
		return;
	}

	// 4. 타일 종류별 규칙 처리
	const ESATileType NewType = TouchedTile->GetTileType();

	// 벽 타일: 절대 지나갈 수 없음
	if (NewType == ESATileType::Wall)
	{
		return;
	}

	// 방해(함정) 타일: "방해 타일에 도착해도 지금까지 선택한 타일들이 해제되게 해줘"
	if (NewType == ESATileType::Trap)
	{
		TriggerTrapHit();
		return;
	}

	// 웨이포인트 타일: "웨이포인트까지 갔다면 터치를 중단해도 지금까지 선택된 건 유지가 되고"
	if (NewType == ESATileType::Waypoint)
	{
		const int32 WaypointIdx = TouchedTile->GetWaypointIndex();
		if (WaypointIdx == NextWaypointIndexToVisit)
		{
			// 올바른 순서의 웨이포인트 도달!
			SelectedPath.Add(TouchedTile);
			TouchedTile->SetSelected(true);
			NextWaypointIndexToVisit++;

			// 체크포인트 저장 (터치를 중단해도 이 웨이포인트까지는 유지됨)
			SavedCheckpointPath = SelectedPath;
			SavedWaypointIndex = NextWaypointIndexToVisit;

			UE_LOG(LogTemp, Log, TEXT("[USATileManager] 웨이포인트 %d 도달 성공! (체크포인트 갱신)"), WaypointIdx);
			OnWaypointReached.Broadcast(WaypointIdx);

			// WayPoint SFX 재생
			if (USASoundManager* SoundManager = USASoundManager::Get(this))
			{
				SoundManager->PlaySound2D(ESASFXType::WayPoint);
			}

			// 카메라 이동 웨이포인트(레벨 16~20용)라면, 이 타일 자신이 아니라 지정된 CameraFocusCoord
			// 위치로 카메라가 이동을 마칠 때까지 터치 입력을 잠근다 (다음 웨이포인트/목표 타일을 미리 보여주기 위함)
			if (TouchedTile->ShouldMoveCameraOnArrival())
			{
				bInputLocked = true;
				const FIntPoint FocusCoord = TouchedTile->GetCameraFocusCoord();
				const FVector FocusWorldLocation = CalculateTileWorldLocation(FocusCoord.X, FocusCoord.Y);
				OnWaypointCameraMoveRequested.Broadcast(FocusWorldLocation);
			}
		}
		else
		{
			// 순서가 맞지 않는 웨이포인트는 지나갈 수 없음!
			UE_LOG(LogTemp, Verbose, TEXT("[USATileManager] 웨이포인트 순서 불일치 (현재 필요: %d, 도달 시도: %d)"),
				NextWaypointIndexToVisit, WaypointIdx);
		}
		return;
	}

	// 목표(Goal) 타일: 모든 웨이포인트를 순서대로 지나왔을 때만 클리어 인정
	if (NewType == ESATileType::Goal)
	{
		const int32 TotalWaypoints = CurrentLevelData.WaypointCoords.Num();
		if (NextWaypointIndexToVisit > TotalWaypoints)
		{
			// 레벨 성공!
			SelectedPath.Add(TouchedTile);
			TouchedTile->SetSelected(true);
			SavedCheckpointPath = SelectedPath;
			bLevelCompleted = true;
			bIsDragging = false;

			UE_LOG(LogTemp, Log, TEXT("[USATileManager] 목표(Goal) 타일 도달! 레벨 %d 클리어!"), CurrentLevelNumber);
			OnLevelCompleted.Broadcast(CurrentLevelNumber);

			// Goal SFX 재생
			if (USASoundManager* SoundManager = USASoundManager::Get(this))
			{
				SoundManager->PlaySound2D(ESASFXType::Goal);
			}
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[USATileManager] 웨이포인트를 모두 방문하지 않아 목표 타일에 들어갈 수 없습니다. (남은 웨이포인트: %d)"),
				TotalWaypoints - NextWaypointIndexToVisit + 1);
		}
		return;
	}

	// 일반 타일 및 기타 타일: 경로에 추가하고 빨간색으로 선택
	SelectedPath.Add(TouchedTile);
	TouchedTile->SetSelected(true);

	// Normal 타일 선택 시 SFX 재생
	if (NewType == ESATileType::Normal)
	{
		if (USASoundManager* SoundManager = USASoundManager::Get(this))
		{
			SoundManager->PlaySound2D(ESASFXType::Normal);
		}
	}
}

void USATileManager::ProcessTileTouchEnd()
{
	// 메뉴 이동 슬라이더를 드래그 중이었다면 그쪽만 마무리 처리하고 레벨 경로는 건드리지 않음
	if (bIsMenuButtonDragging)
	{
		bIsMenuButtonDragging = false;

		const bool bSwiped = MenuButtonSelectedPath.Num() >= 2;
		for (auto& Tile : MenuButtonSelectedPath)
		{
			if (IsValid(Tile))
			{
				Tile->SetSelected(false);
			}
		}
		MenuButtonSelectedPath.Empty();

		// 성공/실패 여부와 무관하게 다시 위쪽 타일 한 칸만 보이는 상태로 접는다 (성공 시에는 곧 ClearTiles로 파괴되므로 무관)
		if (MenuButtonTileBottom)
		{
			MenuButtonTileBottom->SetActorHiddenInGame(true);
			MenuButtonTileBottom->SetActorEnableCollision(false);
		}

		if (bSwiped)
		{
			UE_LOG(LogTemp, Log, TEXT("[USATileManager] 나가기 슬라이더 스와이프 완료! 메뉴로 이동합니다."));
			OnMenuButtonSwiped.Broadcast();
		}
		return;
	}

	bIsDragging = false;

	if (bInputLocked || bLevelCompleted)
	{
		return;
	}

	// "웨이포인트나 목표 타일까지 도착하지 않은 상태에서 터치를 중단하면 지금까지 선택한 타일이 선택 해제되게 하고 싶어"
	// 즉, 마지막 저장된 체크포인트(웨이포인트) 이후에 선택했던 타일들은 모두 선택 해제하고 체크포인트 상태로 롤백!
	if (SelectedPath.Num() > SavedCheckpointPath.Num())
	{
		for (int32 i = SavedCheckpointPath.Num(); i < SelectedPath.Num(); ++i)
		{
			if (IsValid(SelectedPath[i]))
			{
				SelectedPath[i]->SetSelected(false);
			}
		}

		SelectedPath = SavedCheckpointPath;
		NextWaypointIndexToVisit = SavedWaypointIndex;

		UE_LOG(LogTemp, Log, TEXT("[USATileManager] 터치 중단: 웨이포인트 미도달로 마지막 체크포인트 상태로 롤백되었습니다."));
		OnPathReset.Broadcast();
	}
}

void USATileManager::ResetDragPath(bool bResetCheckpoints)
{
	if (bResetCheckpoints)
	{
		for (auto& Tile : SelectedPath)
		{
			if (IsValid(Tile))
			{
				Tile->SetSelected(false);
			}
		}

		SelectedPath.Empty();
		SavedCheckpointPath.Empty();
		NextWaypointIndexToVisit = 1;
		SavedWaypointIndex = 1;
	}
	else
	{
		// 체크포인트 이후만 취소
		for (int32 i = SavedCheckpointPath.Num(); i < SelectedPath.Num(); ++i)
		{
			if (IsValid(SelectedPath[i]))
			{
				SelectedPath[i]->SetSelected(false);
			}
		}

		SelectedPath = SavedCheckpointPath;
		NextWaypointIndexToVisit = SavedWaypointIndex;
	}

	bIsDragging = false;
	OnPathReset.Broadcast();
}

void USATileManager::TriggerTrapHit()
{
	UE_LOG(LogTemp, Warning, TEXT("[USATileManager] 함정 타일에 도달했습니다! 전체 경로가 초기화됩니다."));
	ResetDragPath(true);
	bIsDragging = false;
	OnTrapTriggered.Broadcast();

	// Trap SFX 재생
	if (USASoundManager* SoundManager = USASoundManager::Get(this))
	{
		SoundManager->PlaySound2D(ESASFXType::Trap);
	}
}

void USATileManager::NotifyTileBecameTrapWhileHeld(ASATileActor* Tile)
{
	// 지금 드래그 경로의 "마지막(현재 위치)" 타일일 때만 반응한다.
	// 이미 지나온(경로 중간에 있는) 타일이 나중에 함정으로 바뀌는 것은 지금 밟고 있는 게 아니므로 무시한다.
	if (!Tile || SelectedPath.IsEmpty() || SelectedPath.Last() != Tile)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[USATileManager] 밟고 있던 타일이 함정으로 전환되었습니다! 지금 함정을 밟은 것과 동일하게 처리합니다."));
	TriggerTrapHit();
}

void USATileManager::NotifyMonsterCaughtPlayer()
{
	UE_LOG(LogTemp, Warning, TEXT("[USATileManager] 몬스터가 플레이어를 붙잡았습니다!"));
	OnMonsterCaughtPlayer.Broadcast();
}

void USATileManager::NotifyCameraMoveCompleted()
{
	bInputLocked = false;
	UE_LOG(LogTemp, Log, TEXT("[USATileManager] 카메라 이동 완료! 터치 입력이 다시 활성화되었습니다."));
}

void USATileManager::StartTileWipeEffect()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 드래그 중이던 경로가 있다면 먼저 정리하고, 이후 타일 터치 입력을 전부 무시
	ResetDragPath(true);
	bInputLocked = true;

	// 그리드 중심에서 먼 타일부터(바깥 → 안쪽) 파괴되도록 거리 내림차순으로 정렬
	const FVector Center = GetGridCenterLocation();
	WipeOrderedTiles = GetAllTiles();
	WipeOrderedTiles.Sort([Center](const ASATileActor& A, const ASATileActor& B)
	{
		const float DistA = FVector::DistSquared(A.GetActorLocation(), Center);
		const float DistB = FVector::DistSquared(B.GetActorLocation(), Center);
		return DistA > DistB;
	});

	WipeIndex = 0;

	if (WipeOrderedTiles.IsEmpty())
	{
		// 타일이 아예 없다면 곧바로 완료 처리
		bInputLocked = false;
		OnTileWipeCompleted.Broadcast();
		return;
	}

	// 그리드 크기와 무관하게 전체 연출이 대략 WipeDuration 안에 끝나도록 스텝당 파괴 개수를 역산
	const int32 StepCount = FMath::Max(1, FMath::CeilToInt(WipeDuration / FMath::Max(WipeStepInterval, KINDA_SMALL_NUMBER)));
	WipeTilesPerStep = FMath::Max(1, FMath::CeilToInt((float)WipeOrderedTiles.Num() / (float)StepCount));

	World->GetTimerManager().SetTimer(WipeTimerHandle, this, &USATileManager::ProcessWipeStep, WipeStepInterval, true);
}

void USATileManager::ProcessWipeStep()
{
	const int32 EndIndex = FMath::Min(WipeIndex + WipeTilesPerStep, WipeOrderedTiles.Num());
	for (int32 i = WipeIndex; i < EndIndex; ++i)
	{
		if (ASATileActor* Tile = WipeOrderedTiles[i])
		{
			if (IsValid(Tile))
			{
				SpawnedTileMap.Remove(Tile->GetGridCoord());
				Tile->Destroy();
			}
		}
	}
	WipeIndex = EndIndex;

	if (WipeIndex >= WipeOrderedTiles.Num())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(WipeTimerHandle);
		}

		WipeOrderedTiles.Empty();
		bInputLocked = false;

		// 혹시 남아있을 상태(캐시된 시작/목표 타일, 웨이포인트 맵 등)를 완전히 정리
		ClearTiles();

		UE_LOG(LogTemp, Log, TEXT("[USATileManager] 타일 와이프 연출 완료!"));
		OnTileWipeCompleted.Broadcast();
	}
}

TArray<ASATileActor*> USATileManager::GetSelectedPath() const
{
	TArray<ASATileActor*> Result;
	Result.Reserve(SelectedPath.Num());
	for (const auto& Tile : SelectedPath)
	{
		if (IsValid(Tile))
		{
			Result.Add(Tile.Get());
		}
	}
	return Result;
}

// -------------------------------------------------------------
// 검색 및 조회
// -------------------------------------------------------------

ASATileActor* USATileManager::GetTileAtCoord(const FIntPoint& InCoord) const
{
	if (const TObjectPtr<ASATileActor>* Found = SpawnedTileMap.Find(InCoord))
	{
		return Found->Get();
	}
	return nullptr;
}

ASATileActor* USATileManager::GetTileAt(int32 InX, int32 InY) const
{
	return GetTileAtCoord(FIntPoint(InX, InY));
}

ASATileActor* USATileManager::GetWaypointTile(int32 InWaypointIndex) const
{
	if (const TObjectPtr<ASATileActor>* Found = WaypointTileMap.Find(InWaypointIndex))
	{
		return Found->Get();
	}
	return nullptr;
}

TArray<ASATileActor*> USATileManager::GetAllTiles() const
{
	TArray<ASATileActor*> Result;
	Result.Reserve(SpawnedTileMap.Num());
	for (const auto& Pair : SpawnedTileMap)
	{
		if (IsValid(Pair.Value))
		{
			Result.Add(Pair.Value.Get());
		}
	}
	return Result;
}

FVector USATileManager::CalculateTileWorldLocation(int32 InX, int32 InY) const
{
	float OffsetX = InX * TileStep;
	float OffsetY = InY * TileStep;

	if (bCenterGrid && CurrentLevelData.GridWidth > 0 && CurrentLevelData.GridHeight > 0)
	{
		OffsetX -= ((CurrentLevelData.GridWidth - 1) * 0.5f) * TileStep;
		OffsetY -= ((CurrentLevelData.GridHeight - 1) * 0.5f) * TileStep;
	}

	return GridOrigin + FVector(OffsetX, OffsetY, 0.0f);
}

FVector USATileManager::GetGridCenterLocation() const
{
	if (bCenterGrid)
	{
		return GridOrigin;
	}

	if (CurrentLevelData.GridWidth > 0 && CurrentLevelData.GridHeight > 0)
	{
		const float CenterX = (CurrentLevelData.GridWidth - 1) * 0.5f * TileStep;
		const float CenterY = (CurrentLevelData.GridHeight - 1) * 0.5f * TileStep;
		return GridOrigin + FVector(CenterX, CenterY, 0.0f);
	}

	return GridOrigin;
}

FVector2D USATileManager::GetGridWorldSize() const
{
	const float SizeX = FMath::Max(1, CurrentLevelData.GridWidth) * TileStep;
	const float SizeY = FMath::Max(1, CurrentLevelData.GridHeight) * TileStep;
	return FVector2D(SizeX, SizeY);
}

// -------------------------------------------------------------
// 나가기(메뉴 이동) 슬라이더 (화면 우상단, 플레이어 폰에 부착되어 항상 같은 화면 위치 유지)
// -------------------------------------------------------------

void USATileManager::SpawnMenuButtonTiles()
{
	DestroyMenuButtonTiles();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ASAPlayerPawn* PlayerPawn = Cast<ASAPlayerPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[USATileManager] 플레이어 폰을 찾지 못해 나가기 슬라이더를 배치할 수 없습니다."));
		return;
	}

	// 폰(카메라) 기준 실제 화면 우상단 모서리 위치(그리드 표면보다 MenuButtonHeightOffset만큼 위)를
	// 위쪽 타일 위치로 삼고, 그 아래로 MenuButtonTileGap만큼 떨어진 곳에 (평소 숨겨진) 아래쪽 타일을 배치한다.
	// 레벨 로드 직후처럼 뷰포트 크기를 아직 알 수 없는 시점에는 계산이 실패할 수 있는데, 이때 잘못된 값
	// (예: 폰 위치 그대로)으로 스폰해버리면 나가기 타일이 인게임 그리드 한가운데 겹쳐버리므로, 대신 잠시
	// 뒤 다시 시도한다.
	FVector TopLocalOffset;
	if (!PlayerPawn->GetTopRightCornerOffset(MenuButtonHeightOffset, TopLocalOffset))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[USATileManager] 아직 뷰포트 크기를 알 수 없어 나가기 슬라이더 배치를 0.1초 뒤 재시도합니다."));
		FTimerHandle RetryTimerHandle;
		World->GetTimerManager().SetTimer(RetryTimerHandle, this, &USATileManager::SpawnMenuButtonTiles, 0.1f, false);
		return;
	}

	UClass* ClassToSpawn = TileActorClass ? TileActorClass.Get() : ASATileActor::StaticClass();

	const FVector BottomLocalOffset = TopLocalOffset + FVector(0.0f, MenuButtonTileGap, 0.0f);

	const FVector PawnLocation = PlayerPawn->GetActorLocation();
	const FVector TopWorldLoc = PawnLocation + TopLocalOffset;
	const FVector BottomWorldLoc = PawnLocation + BottomLocalOffset;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	MenuButtonTileTop = World->SpawnActor<ASATileActor>(ClassToSpawn, TopWorldLoc, FRotator::ZeroRotator, SpawnParams);
	MenuButtonTileBottom = World->SpawnActor<ASATileActor>(ClassToSpawn, BottomWorldLoc, FRotator::ZeroRotator, SpawnParams);

	ASATileActor* ButtonTiles[] = { MenuButtonTileTop.Get(), MenuButtonTileBottom.Get() };
	for (ASATileActor* ButtonTile : ButtonTiles)
	{
		if (!ButtonTile)
		{
			continue;
		}

		// 레벨 그리드와 무관한 별도의 UI성 타일이므로 SpawnedTileMap에는 등록하지 않는다 (좌표 조회/와이프 대상에서 제외됨)
		ButtonTile->NormalColor = MenuButtonColor;
		ButtonTile->SelectedColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f);
		ButtonTile->SetTileType(ESATileType::Normal);

		// 카메라가 이동해도 항상 같은 화면 위치를 유지하도록 폰에 붙인다 (KeepWorldTransform: 방금 계산한 월드 위치는 그대로 유지)
		ButtonTile->AttachToActor(PlayerPawn, FAttachmentTransformRules::KeepWorldTransform);
	}

	// 평소에는 위쪽 타일 한 칸만 보이고, 아래쪽 타일은 위쪽을 터치해야 드러난다
	if (MenuButtonTileBottom)
	{
		MenuButtonTileBottom->SetActorHiddenInGame(true);
		MenuButtonTileBottom->SetActorEnableCollision(false);
	}
}

void USATileManager::DestroyMenuButtonTiles()
{
	if (IsValid(MenuButtonTileTop))
	{
		MenuButtonTileTop->Destroy();
	}
	MenuButtonTileTop = nullptr;

	if (IsValid(MenuButtonTileBottom))
	{
		MenuButtonTileBottom->Destroy();
	}
	MenuButtonTileBottom = nullptr;

	bIsMenuButtonDragging = false;
	MenuButtonSelectedPath.Empty();
}

