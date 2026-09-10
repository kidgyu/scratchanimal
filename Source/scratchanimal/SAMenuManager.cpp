// Fill out your copyright notice in the Description page of Project Settings.

#include "SAMenuManager.h"
#include "SATileSettings.h"
#include "SASoundManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

USAMenuManager::USAMenuManager()
{
	MenuTileActorClass = ASAMenuTileActor::StaticClass();
	GridWidth = 10;
	GridHeight = 5;
	TileStep = 105.0f;
	GridOrigin = FVector::ZeroVector;
	bCenterGrid = true;
	bIsDragging = false;

	// 파스텔 톤 무지개색(빨-주-노-초-파-남-보) - 레벨 그리드 한 줄(행)마다 순서대로 순환하며 사용
	RowGroupColors =
	{
		FLinearColor(1.00f, 0.75f, 0.75f, 1.0f), // 파스텔 레드
		FLinearColor(1.00f, 0.85f, 0.65f, 1.0f), // 파스텔 오렌지
		FLinearColor(1.00f, 0.97f, 0.70f, 1.0f), // 파스텔 옐로우
		FLinearColor(0.72f, 0.93f, 0.72f, 1.0f), // 파스텔 그린
		FLinearColor(0.70f, 0.82f, 1.00f, 1.0f), // 파스텔 블루
		FLinearColor(0.75f, 0.72f, 0.95f, 1.0f), // 파스텔 인디고
		FLinearColor(0.93f, 0.72f, 0.95f, 1.0f), // 파스텔 바이올렛
	};
}

void USAMenuManager::Deinitialize()
{
	ClearMenuTiles();
	Super::Deinitialize();
}

USAMenuManager* USAMenuManager::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<USAMenuManager>() : nullptr;
}

void USAMenuManager::LoadMenu()
{
	ClearMenuTiles();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const USATileSettings* Settings = USATileSettings::Get();
	const int32 TotalLevels = Settings ? Settings->GetTotalLevelCount() : 0;

	// 시작 타일: 레벨 그리드보다 한 칸 왼쪽(X = -1)에 예외적으로 배치해서, 레벨 그리드 쪽은 시작 타일 자리를
	// 신경 쓸 필요 없이 한 줄(행)당 정확히 GridWidth개씩 깔끔하게 그룹화될 수 있도록 한다.
	SpawnMenuTile(-1, 0, true, 0);

	// 레벨 그리드: 한 줄당 GridWidth개씩 앞에서부터 순서대로 1..TotalLevels를 배정하고, 남는 칸은 빈 타일로 둔다.
	int32 NextLevelNumber = 1;

	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			int32 LevelNumberForThisTile = 0;
			if (NextLevelNumber <= TotalLevels)
			{
				LevelNumberForThisTile = NextLevelNumber;
				++NextLevelNumber;
			}

			SpawnMenuTile(X, Y, false, LevelNumberForThisTile);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[USAMenuManager] 메뉴 그리드 생성 완료! (그리드: %dx%d, 등록된 레벨: %d개)"), GridWidth, GridHeight, TotalLevels);
}

ASAMenuTileActor* USAMenuManager::SpawnMenuTile(int32 InX, int32 InY, bool bIsStart, int32 InLevelNumber)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ClassToSpawn = MenuTileActorClass ? MenuTileActorClass.Get() : ASAMenuTileActor::StaticClass();
	const FVector SpawnLoc = CalculateTileWorldLocation(InX, InY);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASAMenuTileActor* NewTile = World->SpawnActor<ASAMenuTileActor>(ClassToSpawn, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	if (NewTile)
	{
		// SATileActor의 Start/Normal/Selected 색상과 항상 일치하도록 스폰 시점에 강제로 덮어쓴다
		// (MenuTileActorClass가 만약 기본값이 다른 블루프린트 자식 클래스여도 항상 이 색으로 고정됨)
		NewTile->StartColor = FLinearColor(0.1f, 0.85f, 0.2f, 1.0f);
		NewTile->NormalColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
		NewTile->SelectedColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f);

		// 줄(행)마다 무지개색을 순환 배정 (레벨이 배정된 타일에만 실제로 보임)
		if (!bIsStart && !RowGroupColors.IsEmpty())
		{
			const int32 RowIndex = ((InY % RowGroupColors.Num()) + RowGroupColors.Num()) % RowGroupColors.Num();
			NewTile->LevelColor = RowGroupColors[RowIndex];
		}

		NewTile->SetGridCoord(InX, InY);
		NewTile->SetIsStartTile(bIsStart);
		NewTile->SetAssignedLevelNumber(InLevelNumber);

		SpawnedTileMap.Add(FIntPoint(InX, InY), NewTile);
	}

	return NewTile;
}

void USAMenuManager::ClearMenuTiles()
{
	for (auto& Pair : SpawnedTileMap)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}
	SpawnedTileMap.Empty();

	ResetDragPath();
}

void USAMenuManager::ProcessTileTouchBegin(ASAMenuTileActor* TouchedTile)
{
	if (!TouchedTile || !SelectedPath.IsEmpty())
	{
		return;
	}

	// 반드시 시작 타일부터 드래그를 시작해야 함
	if (TouchedTile->IsStartTile())
	{
		bIsDragging = true;
		SelectedPath.Add(TouchedTile);
		TouchedTile->SetSelected(true);
	}
}

void USAMenuManager::ProcessTileTouchMove(ASAMenuTileActor* TouchedTile)
{
	if (!bIsDragging || !TouchedTile || SelectedPath.IsEmpty())
	{
		return;
	}

	ASAMenuTileActor* LastTile = SelectedPath.Last();

	if (TouchedTile == LastTile)
	{
		return;
	}

	// 직전 타일로 되돌아온 경우 (Undo / 되돌리기 기능) - 시작 타일 자체는 취소할 수 없음
	if (SelectedPath.Num() >= 2 && TouchedTile == SelectedPath[SelectedPath.Num() - 2])
	{
		LastTile->SetSelected(false);
		SelectedPath.Pop();
		return;
	}

	// 인접성 검사: 대각선이나 먼 거리 점프는 인식하지 않음
	if (!AreTilesAdjacent(LastTile, TouchedTile))
	{
		return;
	}

	// 이미 현재 경로에 포함된 타일은 재진입 불가
	if (SelectedPath.Contains(TouchedTile))
	{
		return;
	}

	SelectedPath.Add(TouchedTile);
	TouchedTile->SetSelected(true);

	// SATileActor의 일반 타일 선택 시와 동일한 SFX 재생
	if (USASoundManager* SoundManager = USASoundManager::Get(this))
	{
		SoundManager->PlaySound2D(ESASFXType::Normal);
	}
}

void USAMenuManager::ProcessTileTouchEnd()
{
	bIsDragging = false;

	// 시작 타일만 누르고 뗐을 뿐 실제로 드래그하지 않았다면 그냥 초기화
	if (SelectedPath.Num() < 2)
	{
		ResetDragPath();
		return;
	}

	ASAMenuTileActor* FinalTile = SelectedPath.Last();
	const int32 SelectedLevelNumber = FinalTile ? FinalTile->GetAssignedLevelNumber() : 0;

	if (SelectedLevelNumber > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[USAMenuManager] 레벨 %d 선택됨"), SelectedLevelNumber);
		OnLevelSelected.Broadcast(SelectedLevelNumber);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("[USAMenuManager] 등록된 레벨이 없는 타일에서 드래그를 놓아 선택이 취소되었습니다."));
	}

	// 선택 성공/실패와 무관하게 매 시도가 끝나면 경로 하이라이트는 항상 초기화 (실패 시 요구사항대로 빈 타일 상태로 복귀)
	ResetDragPath();
}

bool USAMenuManager::AreTilesAdjacent(const ASAMenuTileActor* TileA, const ASAMenuTileActor* TileB)
{
	if (!TileA || !TileB)
	{
		return false;
	}

	const FIntPoint CoordA = TileA->GetGridCoord();
	const FIntPoint CoordB = TileB->GetGridCoord();

	const int32 ManhattanDist = FMath::Abs(CoordA.X - CoordB.X) + FMath::Abs(CoordA.Y - CoordB.Y);
	return ManhattanDist == 1;
}

ASAMenuTileActor* USAMenuManager::GetTileAtCoord(const FIntPoint& InCoord) const
{
	if (const TObjectPtr<ASAMenuTileActor>* Found = SpawnedTileMap.Find(InCoord))
	{
		return Found->Get();
	}
	return nullptr;
}

FVector USAMenuManager::CalculateTileWorldLocation(int32 InX, int32 InY) const
{
	float OffsetX = InX * TileStep;
	float OffsetY = InY * TileStep;

	if (bCenterGrid && GridWidth > 0 && GridHeight > 0)
	{
		OffsetX -= ((GridWidth - 1) * 0.5f) * TileStep;
		OffsetY -= ((GridHeight - 1) * 0.5f) * TileStep;
	}

	return GridOrigin + FVector(OffsetX, OffsetY, 0.0f);
}

void USAMenuManager::ResetDragPath()
{
	for (auto& Tile : SelectedPath)
	{
		if (IsValid(Tile))
		{
			Tile->SetSelected(false);
		}
	}

	SelectedPath.Empty();
	bIsDragging = false;
}
