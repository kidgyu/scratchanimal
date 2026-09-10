// Fill out your copyright notice in the Description page of Project Settings.

#include "SAResultManager.h"
#include "SASoundManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

namespace SAResultFont
{
	struct FStroke
	{
		FVector2D A;
		FVector2D B;
	};

	// 문자 로컬 좌표계: 가로 0~4, 세로 0~6 (Y는 아래로 증가). 각 문자는 직선 획(스트로크) 몇 개로만 정의된다 -
	// 도트 매트릭스 비트맵이 아니므로 정수 칸에 맞출 필요가 없고, 대각선도 매끈하게 샘플링된다.
	static const TMap<TCHAR, TArray<FStroke>>& GetLetterStrokes()
	{
		static const TMap<TCHAR, TArray<FStroke>> Strokes = {
			{ TEXT('W'), {
				{ FVector2D(0,0), FVector2D(2,6) },
				{ FVector2D(2,6), FVector2D(4,2) },
				{ FVector2D(4,2), FVector2D(6,6) },
				{ FVector2D(6,6), FVector2D(8,0) },
			}},
			{ TEXT('I'), {
				{ FVector2D(0,0), FVector2D(4,0) },
				{ FVector2D(2,0), FVector2D(2,6) },
				{ FVector2D(0,6), FVector2D(4,6) },
			}},
			{ TEXT('N'), {
				{ FVector2D(0,0), FVector2D(0,6) },
				{ FVector2D(0,0), FVector2D(4,6) },
				{ FVector2D(4,0), FVector2D(4,6) },
			}},
			{ TEXT('L'), {
				{ FVector2D(0,0), FVector2D(0,6) },
				{ FVector2D(0,6), FVector2D(3,6) },
			}},
			{ TEXT('O'), {
				{ FVector2D(1,0), FVector2D(3,0) },
				{ FVector2D(3,0), FVector2D(4,1) },
				{ FVector2D(4,1), FVector2D(4,5) },
				{ FVector2D(4,5), FVector2D(3,6) },
				{ FVector2D(3,6), FVector2D(1,6) },
				{ FVector2D(1,6), FVector2D(0,5) },
				{ FVector2D(0,5), FVector2D(0,1) },
				{ FVector2D(0,1), FVector2D(1,0) },
			}},
			{ TEXT('S'), {
				{ FVector2D(4,0), FVector2D(0,0) },
				{ FVector2D(0,0), FVector2D(0,3) },
				{ FVector2D(0,3), FVector2D(4,3) },
				{ FVector2D(4,3), FVector2D(4,6) },
				{ FVector2D(4,6), FVector2D(0,6) },
			}},
			{ TEXT('E'), {
				{ FVector2D(0,0), FVector2D(0,6) },
				{ FVector2D(0,0), FVector2D(4,0) },
				{ FVector2D(0,3), FVector2D(3,3) },
				{ FVector2D(0,6), FVector2D(4,6) },
			}},
		};
		return Strokes;
	}

	constexpr float LetterWidthUnits = 4.0f;
	constexpr float LetterHeightUnits = 6.0f;

	// W는 대각선 4개가 표준 4칸 너비 안에 몰려 있으면 서로 겹쳐 보이므로, 획 사이 간격을 벌리기 위해 W만 2배 너비(8칸)를 쓴다.
	static float GetLetterWidth(TCHAR Ch)
	{
		return (Ch == TEXT('W')) ? (LetterWidthUnits * 2.0f) : LetterWidthUnits;
	}
}

USAResultManager::USAResultManager()
{
	ResultTileActorClass = ASAResultTileActor::StaticClass();
	GridOrigin = FVector::ZeroVector;
	LetterUnitSpacing = 40.0f;
	StrokeSampleStep = 0.5f;
	LetterGapUnits = 5.0f; // 타일(가로 100유닛) 하나 폭만큼만 글자 사이가 비어 보이도록 역산한 값
	SliderOffsetFromOrigin = FVector2D(0.0f, 250.0f);
	SliderTileGap = 105.0f; // 타일 폭(100)보다 커야 겹치지 않음 - SATileManager의 TileStep(105)과 동일한 "1칸" 간격
	bIsDragging = false;

	WinPatternColor = FLinearColor(0.25f, 0.9f, 0.35f, 1.0f);   // 초록 (승리)
	LosePatternColor = FLinearColor(0.95f, 0.2f, 0.2f, 1.0f);   // 빨강 (패배)
}

void USAResultManager::Deinitialize()
{
	ClearResultTiles();
	Super::Deinitialize();
}

USAResultManager* USAResultManager::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<USAResultManager>() : nullptr;
}

void USAResultManager::ShowResult(ESAResultType InResultType)
{
	ClearResultTiles();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString Word = (InResultType == ESAResultType::Win) ? TEXT("WIN") : TEXT("LOSE");
	const FLinearColor PatternColor = (InResultType == ESAResultType::Win) ? WinPatternColor : LosePatternColor;

	// 배경 없이, 글자 획을 따라 찍힌 점들만 타일로 스폰
	const TArray<FVector2D> Points = BuildLetterPattern(Word);
	for (const FVector2D& Point : Points)
	{
		const FVector WorldLoc = CalculateLetterWorldLocation(Point.X, Point.Y);
		SpawnResultTile(WorldLoc, /*bInPatternLit=*/true, /*bInDraggable=*/false, PatternColor, FIntPoint::ZeroValue);
	}

	// 1x2 드래그 슬라이더 (아이폰 잠금 해제 스타일 - 왼쪽에서 시작해 오른쪽으로 드래그). 글자 배치와는 별개로,
	// GridOrigin 기준 오프셋을 "두 타일의 중심"으로 삼아 좌우로 절반씩 나눠 배치한다.
	const FVector SliderCenterLoc = GridOrigin + FVector(SliderOffsetFromOrigin.X, SliderOffsetFromOrigin.Y, 0.0f);
	const FVector SliderLeftLoc = SliderCenterLoc - FVector(SliderTileGap * 0.5f, 0.0f, 0.0f);
	const FVector SliderRightLoc = SliderCenterLoc + FVector(SliderTileGap * 0.5f, 0.0f, 0.0f);

	SpawnResultTile(SliderLeftLoc, false, true, FLinearColor::White, FIntPoint(0, 0));
	SpawnResultTile(SliderRightLoc, false, true, FLinearColor::White, FIntPoint(1, 0));

	UE_LOG(LogTemp, Log, TEXT("[USAResultManager] 결과 화면 생성 완료! (\"%s\", 타일 %d개)"), *Word, SpawnedTiles.Num());
}

ASAResultTileActor* USAResultManager::SpawnResultTile(const FVector& InWorldLocation, bool bInPatternLit, bool bInDraggable, const FLinearColor& InPatternColor, const FIntPoint& InGridCoord)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ClassToSpawn = ResultTileActorClass ? ResultTileActorClass.Get() : ASAResultTileActor::StaticClass();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASAResultTileActor* NewTile = World->SpawnActor<ASAResultTileActor>(ClassToSpawn, InWorldLocation, FRotator::ZeroRotator, SpawnParams);
	if (NewTile)
	{
		// SATileActor의 Start/Selected 색상과 항상 일치하도록 스폰 시점에 강제로 덮어쓴다
		// (ResultTileActorClass가 만약 기본값이 다른 블루프린트 자식 클래스여도 항상 이 색으로 고정됨)
		NewTile->DraggableColor = FLinearColor(0.1f, 0.85f, 0.2f, 1.0f);
		NewTile->SelectedColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f);
		NewTile->PatternLitColor = InPatternColor;

		NewTile->SetGridCoord(InGridCoord.X, InGridCoord.Y);
		NewTile->SetDraggable(bInDraggable);
		NewTile->SetPatternLit(bInPatternLit);

		SpawnedTiles.Add(NewTile);
	}

	return NewTile;
}

TArray<FVector2D> USAResultManager::BuildLetterPattern(const FString& Word) const
{
	TArray<FVector2D> Points;

	float WordWidth = FMath::Max(0, Word.Len() - 1) * LetterGapUnits;
	for (const TCHAR Ch : Word)
	{
		WordWidth += SAResultFont::GetLetterWidth(Ch);
	}
	const float StartX = -WordWidth * 0.5f;
	const float StartY = -SAResultFont::LetterHeightUnits * 0.5f;

	const float SampleStep = FMath::Max(StrokeSampleStep, 0.05f);

	float CursorX = StartX;
	for (const TCHAR Ch : Word)
	{
		const TArray<SAResultFont::FStroke>* LetterStrokes = SAResultFont::GetLetterStrokes().Find(Ch);
		if (LetterStrokes)
		{
			for (const SAResultFont::FStroke& Stroke : *LetterStrokes)
			{
				const FVector2D A(CursorX + Stroke.A.X, StartY + Stroke.A.Y);
				const FVector2D B(CursorX + Stroke.B.X, StartY + Stroke.B.Y);

				// 획 길이에 맞춰 샘플 개수를 정해서, 대각선이라도 SampleStep 간격(기본 0.5)으로 촘촘하게 점을 찍는다.
				// x좌표가 정수일 필요가 없으므로 1칸씩 움직이지 않고 0.5칸씩 등 자유롭게 이동하며 매끈한 선이 된다.
				const float Length = FVector2D::Distance(A, B);
				const int32 NumSteps = FMath::Max(1, FMath::RoundToInt(Length / SampleStep));
				for (int32 i = 0; i <= NumSteps; ++i)
				{
					const float Alpha = (float)i / (float)NumSteps;
					Points.Add(FMath::Lerp(A, B, Alpha));
				}
			}
		}

		CursorX += SAResultFont::GetLetterWidth(Ch) + LetterGapUnits;
	}

	return Points;
}

void USAResultManager::ClearResultTiles()
{
	for (auto& Tile : SpawnedTiles)
	{
		if (IsValid(Tile))
		{
			Tile->Destroy();
		}
	}
	SpawnedTiles.Empty();

	ResetDragPath();
}

void USAResultManager::ProcessTileTouchBegin(ASAResultTileActor* TouchedTile)
{
	if (!TouchedTile || !SelectedPath.IsEmpty())
	{
		return;
	}

	// 드래그 가능한(슬라이더) 타일에서만 새 드래그가 시작됨
	if (TouchedTile->IsDraggable())
	{
		bIsDragging = true;
		SelectedPath.Add(TouchedTile);
		TouchedTile->SetSelected(true);
	}
}

void USAResultManager::ProcessTileTouchMove(ASAResultTileActor* TouchedTile)
{
	if (!bIsDragging || !TouchedTile || SelectedPath.IsEmpty())
	{
		return;
	}

	ASAResultTileActor* LastTile = SelectedPath.Last();

	if (TouchedTile == LastTile)
	{
		return;
	}

	// 직전 타일로 되돌아온 경우 (Undo / 되돌리기 기능)
	if (SelectedPath.Num() >= 2 && TouchedTile == SelectedPath[SelectedPath.Num() - 2])
	{
		LastTile->SetSelected(false);
		SelectedPath.Pop();
		return;
	}

	// 드래그 불가능한(표시 전용) 타일은 절대 경로에 들어올 수 없음
	if (!TouchedTile->IsDraggable())
	{
		return;
	}

	// 인접성 검사: 대각선이나 먼 거리 점프는 인식하지 않음
	if (!AreTilesAdjacent(LastTile, TouchedTile))
	{
		return;
	}

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

void USAResultManager::ProcessTileTouchEnd()
{
	bIsDragging = false;

	// 슬라이더 타일 두 개(시작 + 도착)를 모두 거쳐야만 "잠금 해제" 성공
	if (SelectedPath.Num() >= 2)
	{
		UE_LOG(LogTemp, Log, TEXT("[USAResultManager] 슬라이더 잠금 해제 성공!"));
		OnResultUnlockSwiped.Broadcast();
	}

	ResetDragPath();
}

bool USAResultManager::AreTilesAdjacent(const ASAResultTileActor* TileA, const ASAResultTileActor* TileB)
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

FVector USAResultManager::CalculateLetterWorldLocation(float InLocalX, float InLocalY) const
{
	return GridOrigin + FVector(InLocalX * LetterUnitSpacing, InLocalY * LetterUnitSpacing, 0.0f);
}

void USAResultManager::ResetDragPath()
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
