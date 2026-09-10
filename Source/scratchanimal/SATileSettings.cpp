// Fill out your copyright notice in the Description page of Project Settings.

#include "SATileSettings.h"

USATileSettings::USATileSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("SATileSettings");

	if (LevelDataList.IsEmpty())
	{
		GenerateDefaultLevels();
	}
}

const USATileSettings* USATileSettings::Get()
{
	return GetDefault<USATileSettings>();
}

bool USATileSettings::GetLevelData(int32 InLevelNumber, FSATileLevelData& OutLevelData) const
{
	if (LevelDataList.IsEmpty())
	{
		const_cast<USATileSettings*>(this)->GenerateDefaultLevels();
	}

	for (const FSATileLevelData& Data : LevelDataList)
	{
		if (Data.LevelNumber == InLevelNumber)
		{
			OutLevelData = Data;
			return true;
		}
	}
	return false;
}

void USATileSettings::GenerateDefaultLevels()
{
	LevelDataList.Empty();
	LevelDataList.Reserve(50);

	auto AddLevel = [this](int32 Level, int32 W, int32 H,
		FIntPoint Start, FIntPoint Goal,
		TArray<FIntPoint> Waypoints,
		TArray<FIntPoint> Walls,
		TArray<FIntPoint> Traps,
		TArray<ESATileTrapState> TrapInitialStates = {},
		TArray<float> TrapNormalDurations = {},
		TArray<float> TrapDurations = {},
		TArray<float> TrapInitialDelays = {},
		int32 MonsterCount = 0,
		TArray<FIntPoint> MonsterSpawnCoords = {},
		TArray<bool> WaypointMovesCamera = {},
		TArray<FIntPoint> WaypointCameraFocusCoords = {})
	{
		FSATileLevelData Data;
		Data.LevelNumber = Level;
		Data.GridWidth = W;
		Data.GridHeight = H;
		Data.StartCoord = Start;
		Data.GoalCoord = Goal;
		Data.WaypointCoords = MoveTemp(Waypoints);
		Data.WallCoords = MoveTemp(Walls);
		Data.TrapCoords = MoveTemp(Traps);
		Data.TrapInitialStates = MoveTemp(TrapInitialStates);
		Data.TrapNormalDurations = MoveTemp(TrapNormalDurations);
		Data.TrapDurations = MoveTemp(TrapDurations);
		Data.TrapInitialDelays = MoveTemp(TrapInitialDelays);
		Data.MonsterCount = MonsterCount;
		Data.MonsterSpawnCoords = MoveTemp(MonsterSpawnCoords);
		Data.WaypointMovesCamera = MoveTemp(WaypointMovesCamera);
		Data.WaypointCameraFocusCoords = MoveTemp(WaypointCameraFocusCoords);
		LevelDataList.Add(Data);
	};

	// Y행 전체(X0~X1)를 벽으로 채우되, OpenGateXs는 뚫린 통로로, TrapGateXs는 깜빡이는 함정 통로(Normal↔Trap)로 남겨 "문"을 만든다.
	// 밴드 하나가 그리드를 위/아래 두 구역으로 완전히 갈라놓으므로, 게이트만 지나면 나머지 구역은 자유롭게 이동 가능하다.
	auto HWall = [](TArray<FIntPoint>& Walls, TArray<FIntPoint>& Traps, int32 Y, int32 X0, int32 X1, const TArray<int32>& OpenGateXs, const TArray<int32>& TrapGateXs)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			if (TrapGateXs.Contains(X)) Traps.Add(FIntPoint(X, Y));
			else if (!OpenGateXs.Contains(X)) Walls.Add(FIntPoint(X, Y));
		}
	};

	// X열 전체(Y0~Y1)를 벽으로 채우되, OpenGateYs/TrapGateYs만 통로(또는 함정 통로)로 남겨 좌우 구역을 가르는 "문"을 만든다.
	auto VWall = [](TArray<FIntPoint>& Walls, TArray<FIntPoint>& Traps, int32 X, int32 Y0, int32 Y1, const TArray<int32>& OpenGateYs, const TArray<int32>& TrapGateYs)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			if (TrapGateYs.Contains(Y)) Traps.Add(FIntPoint(X, Y));
			else if (!OpenGateYs.Contains(Y)) Walls.Add(FIntPoint(X, Y));
		}
	};

	// 벽 없이 한 행 전체(X0~X1)를 함정 타일로 채운다. 구간 전체가 함정이라 "아무 칸으로나 건너가도 되지만
	// 안전한 순간(Normal)에 맞춰야 하는" 개방형 함정 띠 - 좁은 문 하나로 몰아넣는 대신 폭 전체가 진입 가능.
	auto FullTrapRow = [](TArray<FIntPoint>& Traps, int32 Y, int32 X0, int32 X1)
	{
		for (int32 X = X0; X <= X1; ++X) Traps.Add(FIntPoint(X, Y));
	};

	// 벽 없이 한 열 전체(Y0~Y1)를 함정 타일로 채운다 (FullTrapRow의 세로 버전)
	auto FullTrapCol = [](TArray<FIntPoint>& Traps, int32 X, int32 Y0, int32 Y1)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y) Traps.Add(FIntPoint(X, Y));
	};

	// "폭탄 피하기" 회피 구간: 사각형(X0~X1, Y0~Y1) 전체를 함정 타일로 채우고, (BlockW x BlockH) 크기의 블록 단위로 묶어
	// 각 블록에 그룹 위상을 부여한다 (같은 위상을 가진 블록들은 항상 동시에 Trap↔Normal 전환되도록 의도된 것이었으나,
	// 현재 ASATileActor는 개별 Normal/Trap 유지 시간 기반으로 전환되므로 이 함수는 사용되지 않음 - 미사용 상태로 남겨둠).
	// 예) PhaseCount=2, 블록폭 2칸이면 "1,2 Trap / 3,4 Normal / 5,6 Trap / 7,8 Normal"처럼 시작해서 1초 뒤 통째로 뒤바뀌는 패턴이 되고,
	// BlockW=BlockH=2 또는 3이면 2x2/3x3 정사각형 블록이 체커보드처럼 함께 깜빡인다. PhaseCount=3이면 셋 중 하나만 안전(Normal)해지는 더 어려운 패턴.
	auto BombDodgeZone = [](TArray<FIntPoint>& Traps, TArray<int32>& GroupPhases, TArray<int32>& PhaseCounts, int32 X0, int32 X1, int32 Y0, int32 Y1, int32 BlockW, int32 BlockH, int32 PhaseCount)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			const int32 BlockCol = (X - X0) / BlockW;
			for (int32 Y = Y0; Y <= Y1; ++Y)
			{
				const int32 BlockRow = (Y - Y0) / BlockH;
				const int32 GroupIndex = BlockCol + BlockRow;
				const int32 GroupPhase = (GroupIndex + 1) % PhaseCount;
				Traps.Add(FIntPoint(X, Y));
				GroupPhases.Add(GroupPhase);
				PhaseCounts.Add(PhaseCount);
			}
		}
	};

	// 두꺼운 가로 벽 블록: Y0~Y1 전체 행(그리드 폭 전체 X범위)을 벽으로 채우되, GateX0~GateX1 구간(전체 Y0~Y1 두께만큼)은
	// 벽을 세우지 않고 구멍으로 남겨둔다. 이 구멍은 반드시 BombDodgeZone으로 같은 범위를 채워 회피 구간으로 만들어야 한다
	// (그래야 벽으로 막힌 두 구역이 회피 패턴을 통해서만 연결됨).
	auto ThickWallBandH = [](TArray<FIntPoint>& Walls, int32 Y0, int32 Y1, int32 GridW, int32 GateX0, int32 GateX1)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = 0; X < GridW; ++X)
			{
				if (X < GateX0 || X > GateX1) Walls.Add(FIntPoint(X, Y));
			}
		}
	};

	// 두꺼운 세로 벽 블록 (ThickWallBandH의 좌우 버전): X0~X1 전체 열을 벽으로 채우되 GateY0~GateY1 구간은 구멍으로 남긴다.
	auto ThickWallBandV = [](TArray<FIntPoint>& Walls, int32 X0, int32 X1, int32 GridH, int32 GateY0, int32 GateY1)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			for (int32 Y = 0; Y < GridH; ++Y)
			{
				if (Y < GateY0 || Y > GateY1) Walls.Add(FIntPoint(X, Y));
			}
		}
	};

	// 한 구역(가로 10칸) 안에 ㄱ/ㄴ/ㄷ 모양의 "모서리" 장애물 3개를 배치한다. 기존의 세로 일자 벽과 달리
	// 이 장애물들은 어느 방향으로도 완전히 막지 않는 코너/요철 모양이라, 항상 옆이나 위아래로 돌아갈 수
	// 있다 (여러 방향에서 접근 가능). ZoneOffsetX만큼 오른쪽으로 옮겨서 여러 구역에 반복 배치할 수 있고,
	// bMirrorY가 true면 세로로 뒤집어서(Y -> 9-Y) 배치해 레벨마다 살짝 다른 모양을 낼 수 있다.
	auto AddZoneCornerWalls = [](TArray<FIntPoint>& Walls, int32 ZoneOffsetX, bool bMirrorY)
	{
		auto Coord = [ZoneOffsetX, bMirrorY](int32 LocalX, int32 LocalY)
		{
			return FIntPoint(ZoneOffsetX + LocalX, bMirrorY ? (9 - LocalY) : LocalY);
		};

		// ㄱ(기역): 가로 팔이 왼쪽으로, 세로 팔이 아래로 뻗는 모서리
		Walls.Append({ Coord(1, 1), Coord(2, 1), Coord(3, 1), Coord(3, 2), Coord(3, 3) });
		// ㄴ(니은): 세로 팔이 위로, 가로 팔이 오른쪽으로 뻗는 모서리
		Walls.Append({ Coord(6, 6), Coord(6, 7), Coord(6, 8), Coord(7, 8), Coord(8, 8) });
		// ㄷ(디귿): 위아래 가로 변 + 왼쪽 세로 변 일부. 왼쪽 세로 변 가운데 칸(로컬 1,6)은 비워둬서,
		// 안쪽 공간이 오른쪽뿐 아니라 왼쪽으로도 통하게 했다 - 두 방향 다 열려 있어야 어느 한쪽 입구가
		// 유일한 통로가 되는 경우가 없어, 웨이포인트가 이 안쪽에 있어도 체크포인트 이후 다른 쪽으로 빠져나갈 수 있다.
		Walls.Append({ Coord(1, 5), Coord(1, 7), Coord(2, 5), Coord(3, 5), Coord(2, 7), Coord(3, 7) });
	};

	// 사각형(X0~X1, Y0~Y1) 트랩 구역을 Y축 방향으로 GroupRowSize줄씩 묶어 위에서부터 아래로 순서대로
	// "정확히 한 그룹만" Trap(위험) 상태가 되는 웨이브 패턴을 만든다. 그룹이 총 N개면 각 그룹은
	// TrapDuration초 동안만 위험하고 나머지 (N-1)*TrapDuration초 동안은 안전(Normal)하며, 그룹 순서만큼
	// InitialDelay를 어긋나게 주어 한 주기(N * TrapDuration초) 내내 항상 한 그룹만 위험 상태가 되도록 한다.
	auto SequentialWaveRows = [](TArray<FIntPoint>& Traps, TArray<ESATileTrapState>& InitStates, TArray<float>& NormalDurs, TArray<float>& TrapDurs, TArray<float>& InitialDelays,
		int32 X0, int32 X1, int32 Y0, int32 Y1, int32 GroupRowSize, float TrapDuration)
	{
		const int32 GroupCount = FMath::Max(1, (Y1 - Y0 + 1) / GroupRowSize);
		const float NormalDuration = FMath::Max(1, GroupCount - 1) * TrapDuration;
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			const int32 GroupIndex = (Y - Y0) / GroupRowSize;
			for (int32 X = X0; X <= X1; ++X)
			{
				Traps.Add(FIntPoint(X, Y));
				InitStates.Add(ESATileTrapState::Trap);
				NormalDurs.Add(NormalDuration);
				TrapDurs.Add(TrapDuration);
				InitialDelays.Add(GroupIndex * TrapDuration);
			}
		}
	};

	// SequentialWaveRows의 가로 버전 - X축 방향으로 GroupColSize칸씩 묶어 왼쪽부터 오른쪽으로 순서대로
	// "정확히 한 그룹만" Trap 상태가 되는 웨이브 패턴 (세로/가로로 90도 돌린 레벨 쌍을 만들 때 사용)
	auto SequentialWaveCols = [](TArray<FIntPoint>& Traps, TArray<ESATileTrapState>& InitStates, TArray<float>& NormalDurs, TArray<float>& TrapDurs, TArray<float>& InitialDelays,
		int32 X0, int32 X1, int32 Y0, int32 Y1, int32 GroupColSize, float TrapDuration)
	{
		const int32 GroupCount = FMath::Max(1, (X1 - X0 + 1) / GroupColSize);
		const float NormalDuration = FMath::Max(1, GroupCount - 1) * TrapDuration;
		for (int32 X = X0; X <= X1; ++X)
		{
			const int32 GroupIndex = (X - X0) / GroupColSize;
			for (int32 Y = Y0; Y <= Y1; ++Y)
			{
				Traps.Add(FIntPoint(X, Y));
				InitStates.Add(ESATileTrapState::Trap);
				NormalDurs.Add(NormalDuration);
				TrapDurs.Add(TrapDuration);
				InitialDelays.Add(GroupIndex * TrapDuration);
			}
		}
	};

	// =================================================================================
	// 1단계 (Lv 1~5, 5x5): 튜토리얼 - 웨이포인트 → 벽 → "깜빡이는 함정 문" 순서로 하나씩 규칙을 학습시킨다.
	// =================================================================================

	// Level 1: 아무 장애물 없는 순수 이동 튜토리얼
	AddLevel(1, 5, 5, FIntPoint(0, 0), FIntPoint(4, 4), {}, {}, {});

	// Level 2: 웨이포인트 개념만 추가
	AddLevel(2, 5, 5, FIntPoint(0, 0), FIntPoint(4, 4),
		{ FIntPoint(4, 0) }, {}, {});

	// Level 3: 벽 + "뚫린" 문 하나 - 반드시 문을 통해서만 반대편으로 이동 가능
	{
		TArray<FIntPoint> Walls, Traps;
		VWall(Walls, Traps, 2, 0, 4, { 2 }, {});
		AddLevel(3, 5, 5, FIntPoint(0, 0), FIntPoint(4, 4),
			{ FIntPoint(4, 0) }, Walls, Traps);
	}

	// Level 4: 첫 "함정 문" 등장 - 문이 Normal일 때만 건널 수 있고, 건너는 순간 그 타일은 선택되어 깜빡임이 멈춘다
	{
		TArray<FIntPoint> Walls, Traps;
		VWall(Walls, Traps, 2, 0, 4, {}, { 2 });
		AddLevel(4, 5, 5, FIntPoint(0, 0), FIntPoint(4, 4), {}, Walls, Traps);
	}

	// Level 5: 함정 문 2개 연속 통과 - 두 함정 모두 Trap 상태로 시작해서 2초간 유지된 뒤, 0.5초의 짧은 Normal(통과 가능) 틈만 열리는 타이밍 챌린지
	{
		TArray<FIntPoint> Walls, Traps;
		VWall(Walls, Traps, 1, 0, 4, {}, { 1 });
		VWall(Walls, Traps, 3, 0, 4, {}, { 3 });
		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Trap, ESATileTrapState::Trap };
		TArray<float> NormalDurs = { 0.5f, 0.5f };
		TArray<float> TrapDurs = { 2.0f, 2.0f };
		AddLevel(5, 5, 5, FIntPoint(0, 0), FIntPoint(4, 4),
			{ FIntPoint(2, 1) }, Walls, Traps, InitStates, NormalDurs, TrapDurs);
	}

	// =================================================================================
	// 2단계 (Lv 6~10): 그리드가 커지고, 벽이 직선 하나짜리 "문"을 넘어 L자/십자/지그재그 등 다양한 모양으로 등장하며,
	// 함정도 단일 문 → 전체 폭 함정 띠 → 딜레이를 이용한 교차 깜빡임까지 패턴이 다양해진다.
	// =================================================================================

	// Level 6 (6x6): "L자" 모양 벽 - 단일 트랩 문(3,4단계와 동일한 기본 타이밍)을 지나 오른쪽 벽의 홈을 통해 목표에 도달
	{
		TArray<FIntPoint> Walls, Traps;
		VWall(Walls, Traps, 3, 0, 5, {}, { 2 }); // (3,2) 한 칸만 트랩 문, 나머지는 벽
		// 오른쪽 벽면에 세로로 긴 홈을 파서 위/아래로 우회하게 만드는 "ㄴ자" 모양
		Walls.Append({ FIntPoint(5, 1), FIntPoint(5, 2), FIntPoint(5, 3), FIntPoint(5, 4) });
		AddLevel(6, 6, 6, FIntPoint(0, 0), FIntPoint(5, 5),
			{ FIntPoint(1, 4) }, Walls, Traps);
	}

	// Level 7 (6x7): 벽 없이 폭 전체를 가로지르는 "함정 띠"(FullTrapRow) 첫 등장 + 위아래로 어긋난 지그재그 벽
	{
		TArray<FIntPoint> Walls, Traps;
		HWall(Walls, Traps, 1, 0, 3, {}, {});   // 위쪽 지그재그: 왼쪽 절반만 벽, 오른쪽(4~5)으로 우회
		FullTrapRow(Traps, 3, 0, 5);            // 그리드 폭 전체(0~5)가 함정 띠 - 아무 칸이나 안전할 때 건너면 됨
		HWall(Walls, Traps, 4, 2, 5, {}, {});   // 아래쪽 지그재그: 오른쪽 절반만 벽, 왼쪽(0~1)으로 우회
		TArray<ESATileTrapState> InitStates;
		TArray<float> NormalDurs, TrapDurs;
		for (int32 i = 0; i < 6; ++i)
		{
			InitStates.Add(ESATileTrapState::Normal);
			NormalDurs.Add(1.2f);
			TrapDurs.Add(1.0f);
		}
		AddLevel(7, 6, 7, FIntPoint(0, 0), FIntPoint(5, 6),
			{ FIntPoint(5, 1) }, Walls, Traps, InitStates, NormalDurs, TrapDurs);
	}

	// Level 8 (7x7): 두 벽이 "십자" 모양으로 교차하며 4개 구역을 나누고, 각 벽의 통로를 서로 다른 위치에 어긋나게 두어
	// (교차점 자체를 문으로 쓰면 사방이 막혀버리므로) S자 경로를 만든다. 두 번째 문은 기존보다 빠르게 깜빡이는 트랩 문.
	{
		TArray<FIntPoint> Walls, Traps;
		VWall(Walls, Traps, 3, 0, 6, { 1 }, {});   // 세로 벽: (3,1)만 뚫린 통로
		HWall(Walls, Traps, 3, 0, 6, {}, { 5 });   // 가로 벽: (5,3)만 트랩 문
		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Trap };
		TArray<float> NormalDurs = { 0.5f };
		TArray<float> TrapDurs = { 0.8f };
		AddLevel(8, 7, 7, FIntPoint(0, 0), FIntPoint(6, 6),
			{ FIntPoint(5, 1) }, Walls, Traps, InitStates, NormalDurs, TrapDurs);
	}

	// Level 9 (7x8): 딜레이를 활용한 "교차 깜빡임" 패턴 - 3,4번 타일은 지연 없이 1초씩 반복하는 기본 트랩 쌍이고,
	// 5,6번 타일은 동일하게 1초씩 반복하되 시작 딜레이를 1초 주어, 3,4번과 정확히 반대 위상으로 어긋나 교차하며 깜빡인다.
	// 가로 벽(2, 4, 5, 6열은 문/함정 자리라 벽을 두지 않음)으로 위/아래 구역을 나누되, 문 폭 전체(3~6)가 함정으로 되어 있어
	// 어느 칸으로 건너도 상관없다.
	{
		TArray<FIntPoint> Walls, Traps;
		HWall(Walls, Traps, 4, 0, 6, {}, { 3, 4, 5, 6 }); // (0,4)(1,4)(2,4)는 벽, (3,4)(4,4)(5,4)(6,4)는 트랩
		// Traps 배열 순서: [ (3,4), (4,4), (5,4), (6,4) ]
		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Trap, ESATileTrapState::Trap, ESATileTrapState::Trap, ESATileTrapState::Trap };
		TArray<float> NormalDurs = { 1.0f, 1.0f, 1.0f, 1.0f };
		TArray<float> TrapDurs = { 1.0f, 1.0f, 1.0f, 1.0f };
		TArray<float> InitialDelays = { 0.0f, 0.0f, 1.0f, 1.0f }; // 3,4번은 딜레이 없음 / 5,6번은 1초 딜레이 후 시작 → 서로 교차
		AddLevel(9, 7, 8, FIntPoint(0, 0), FIntPoint(6, 7),
			{ FIntPoint(6, 1) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays);
	}

	// Level 10 (8x8): 이번 5개 레벨의 종합 - 첫 번째 세로 벽은 기본 타이밍의 단일 트랩 문, 두 번째 세로 벽은
	// 2칸짜리 트랩 문인데 그중 한 칸에만 1초 딜레이를 주어 서로 교차하며 깜빡이는 작은 "교차 쌍"을 재현한다.
	// 중간 구역에는 벽 기둥 하나를 더 두어 벽 모양에 변화를 준다.
	{
		TArray<FIntPoint> Walls, Traps;
		VWall(Walls, Traps, 2, 0, 7, {}, { 2 });      // 첫 번째 문: (2,2) 기본 트랩
		VWall(Walls, Traps, 5, 0, 7, {}, { 5, 6 });   // 두 번째 문: (5,5), (5,6) 두 칸짜리 트랩 문
		Walls.Add(FIntPoint(3, 4));                    // 중간 구역의 장식용 벽 기둥 하나
		// Traps 배열 순서: [ (2,2), (5,5), (5,6) ]
		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Trap, ESATileTrapState::Trap, ESATileTrapState::Trap };
		TArray<float> NormalDurs = { 1.0f, 1.0f, 1.0f };
		TArray<float> TrapDurs = { 1.0f, 1.0f, 1.0f };
		TArray<float> InitialDelays = { 0.0f, 0.0f, 1.0f }; // (5,6)만 1초 딜레이를 주어 (5,5)와 교차하며 깜빡임
		AddLevel(10, 8, 8, FIntPoint(0, 0), FIntPoint(7, 7),
			{ FIntPoint(1, 6) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays);
	}

	// =================================================================================
	// 3단계 (Lv 11~15, 8x8 ~ 10x10): SAMonsterActor 등장 - 타이밍/경로 퍼즐에 더해, A*로 플레이어의
	// 현재 위치(선택 경로의 마지막 칸)를 계속 추격하는 몬스터를 피해야 한다. 11~13은 1마리, 14~15는 2마리.
	// 목표 타일도 더 이상 항상 맨 끝 코너가 아니라 벽 구조에 따라 다양한 위치에 놓이고, 몬스터는
	// MonsterSpawnCoords로 통로의 길목(챕트포인트)에 미리 배치해 두어 지정된 자리를 지키고 서 있는다
	// (레벨이 다시 로드되면 각 몬스터가 이 좌표로 재배치된다).
	// =================================================================================

	// Level 11 (8x8): 몬스터 첫 등장 - 가로 벽 하나로 위/아래를 가르되, 트랩 문을 서로 반대 위상으로
	// 깜빡이는 두 곳(2,4)/(5,4)에 두어 몬스터가 한쪽을 막고 있어도 반드시 다른 한쪽은 항상 열려 있다.
	{
		TArray<FIntPoint> Walls, Traps;
		HWall(Walls, Traps, 4, 0, 7, {}, { 2, 5 }); // (2,4), (5,4) 트랩 문 두 곳 - 그 외는 벽
		// Traps 배열 순서: [ (2,4), (5,4) ]
		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Trap, ESATileTrapState::Trap };
		TArray<float> NormalDurs = { 0.75f, 0.75f };
		TArray<float> TrapDurs = { 0.75f, 0.75f };
		TArray<float> InitialDelays = { 0.0f, 0.75f }; // 정확히 반대 위상 - 둘 중 하나는 항상 통과 가능
		AddLevel(11, 8, 8, FIntPoint(0, 0), FIntPoint(4, 7),
			{ FIntPoint(7, 1) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 1,
			{ FIntPoint(4, 6) }); // 몬스터가 목표 바로 앞을 지키고 서 있음 (문 중 하나가 막혀도 다른 문으로 우회 가능)
	}

	// Level 12 (8x9): 몬스터 1마리 + 함정 없이 순수 벽 구조로 만든 S자 통로 - 각 통로를 2칸 폭으로 넓혀서
	// 몬스터가 한 칸을 막고 서 있어도 옆 칸으로 빠져나갈 수 있게 했다.
	{
		TArray<FIntPoint> Walls, Traps;
		HWall(Walls, Traps, 3, 0, 7, { 6, 7 }, {});   // (6,3),(7,3) 두 칸 통로
		HWall(Walls, Traps, 6, 0, 7, { 0, 1 }, {});   // (0,6),(1,6) 두 칸 통로
		AddLevel(12, 8, 9, FIntPoint(0, 0), FIntPoint(2, 8),
			{ FIntPoint(7, 2) }, Walls, Traps, {}, {}, {}, {}, 1,
			{ FIntPoint(1, 7) }); // 마지막 구역, 목표 근처에서 매복
	}

	// Level 13 (9x9): 몬스터 1마리 + "십자" 모양 교차 벽 - 목표를 시작과 같은 줄(위쪽) 반대편 끝에 두어
	// 세로 벽의 2칸 통로로 곧장 건너가게 하고, 가로 벽 양쪽에 각각 트랩 문을 하나씩 두어(왼쪽/오른쪽)
	// 좌하단·우하단 구역 모두 오갈 수 있는 선택지를 만든다.
	{
		TArray<FIntPoint> Walls, Traps;
		VWall(Walls, Traps, 4, 0, 8, { 1, 2 }, {});          // 세로 벽: (4,1),(4,2) 두 칸 통로
		HWall(Walls, Traps, 4, 0, 8, {}, { 1, 2, 5, 6 });    // 가로 벽: 왼쪽 (1,4)/(2,4), 오른쪽 (5,4)/(6,4) - 각각 2칸씩
		// Traps 배열 순서: [ (1,4), (2,4), (5,4), (6,4) ]
		// 웨이포인트가 있는 왼쪽 구역은 들어올 때 쓴 칸이 이미 경로에 포함되어 재진입이 불가능해지므로,
		// 반드시 서로 다른 두 칸(1,4)/(2,4)이 있어야 나갈 때 다른 쪽 칸을 사용해 빠져나올 수 있다.
		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Trap, ESATileTrapState::Trap, ESATileTrapState::Trap, ESATileTrapState::Trap };
		TArray<float> NormalDurs = { 0.5f, 0.5f, 0.5f, 0.5f };
		TArray<float> TrapDurs = { 0.5f, 0.5f, 0.5f, 0.5f };
		TArray<float> InitialDelays = { 0.0f, 0.5f, 0.0f, 0.5f }; // 각 쌍이 정확히 반대 위상 - 항상 한쪽은 통과 가능
		AddLevel(13, 9, 9, FIntPoint(0, 0), FIntPoint(8, 0),
			{ FIntPoint(2, 5) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 1,
			{ FIntPoint(5, 1) }); // 세로 벽의 2칸 통로를 건너오자마자 마주치는 자리 (목표 진입로를 지킴)
	}

	// Level 14 (9x10): 몬스터 2마리 등장 - 서로 반대쪽이 뚫린 가로 벽 두 줄(열린 통로/트랩 문이 좌우로
	// 엇갈림)이라 어느 쪽으로 건널지 경로 선택이 생기고, 두 몬스터가 각 벽의 통로 쪽을 하나씩 지킨다.
	{
		TArray<FIntPoint> Walls, Traps;
		HWall(Walls, Traps, 3, 0, 8, { 1 }, { 7 }); // 열린 통로 (1,3) / 트랩 문 (7,3)
		HWall(Walls, Traps, 6, 0, 8, { 7 }, { 1 }); // 열린 통로 (7,6) / 트랩 문 (1,6)
		// Traps 배열 순서: [ (7,3), (1,6) ]
		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Trap, ESATileTrapState::Trap };
		TArray<float> NormalDurs = { 1.0f, 1.0f };
		TArray<float> TrapDurs = { 1.0f, 1.0f };
		TArray<float> InitialDelays = { 0.0f, 0.5f };
		AddLevel(14, 9, 10, FIntPoint(0, 0), FIntPoint(4, 9),
			{ FIntPoint(8, 1) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 2,
			{ FIntPoint(7, 4), FIntPoint(1, 7) }); // 각각 두 번째 벽의 열린 통로 쪽 / 목표 근처를 지킴
	}

	// Level 15 (10x10): 3단계 종합 - 세로 트랩 문 두 곳을 각각 2칸짜리 교차 깜빡임 쌍으로 만들어, 몬스터가
	// 한쪽 칸을 막고 서 있어도 반드시 다른 한쪽은 열려 있게 했다. 오른쪽 끝의 웨이포인트를 반드시 들러야
	// 해서 두 문을 왕복으로 건너야 하고, 몬스터 2마리가 각 문 통과 직후 자리를 지킨다.
	{
		TArray<FIntPoint> Walls, Traps;
		VWall(Walls, Traps, 3, 0, 9, {}, { 4, 5 });   // 첫 번째 문: (3,4), (3,5) 교차 깜빡이는 2칸
		VWall(Walls, Traps, 7, 0, 9, {}, { 6, 7 });   // 두 번째 문: (7,6), (7,7) 교차 깜빡이는 2칸
		Walls.Add(FIntPoint(5, 5));                    // 중간 구역의 장식용 벽 기둥 하나
		// Traps 배열 순서: [ (3,4), (3,5), (7,6), (7,7) ]
		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Trap, ESATileTrapState::Trap, ESATileTrapState::Trap, ESATileTrapState::Trap };
		TArray<float> NormalDurs = { 1.0f, 1.0f, 1.0f, 1.0f };
		TArray<float> TrapDurs = { 1.0f, 1.0f, 1.0f, 1.0f };
		TArray<float> InitialDelays = { 0.0f, 1.0f, 0.0f, 1.0f }; // 각 문마다 두 칸이 정확히 반대 위상 - 항상 한쪽은 통과 가능
		AddLevel(15, 10, 10, FIntPoint(0, 0), FIntPoint(0, 9),
			{ FIntPoint(9, 3) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 2,
			{ FIntPoint(4, 4), FIntPoint(8, 7) }); // 각각 첫 번째/두 번째 문을 건너자마자 마주치는 자리 (한쪽 칸이 막혀도 다른 칸으로 우회 가능)
	}

	// =================================================================================
	// 4단계 (Lv 16~20, 20x10): 15레벨 기준 가로로만 2배 넓힌 20x10 그리드. 세로로 쭉 뻗어 전체를
	// 가로막던 기존 벽 대신, ㄱ/ㄴ/ㄷ 모양의 "모서리" 장애물(AddZoneCornerWalls)만 배치해 어느 방향으로도
	// 완전히 막히지 않고 여러 경로로 돌아갈 수 있게 했다(트랩 문도 필요 없어져 제거). 좌측 구역(X 0~9)
	// 끝의 첫 번째 웨이포인트에서는 화면이 우측 구역(X 10~19)으로 넘어가기 전에 카메라가 먼저 이동해
	// 미리 보여준다. 몬스터는 총 4마리이며, 시작 지점과 구역 진입 지점에서 충분히 떨어진 곳에 배치해
	// 초반 난이도를 낮췄다. 단, 마지막 20레벨만 예외로 몬스터 한 마리를 목표 타일 바로 옆에 두어
	// 마무리 난이도를 유지한다. 레벨마다 장애물을 세로로 뒤집어(bMirrorY) 살짝 다른 모양을 준다.
	// 주의: ㄷ 모양은 왼쪽 세로 변 가운데 칸을 일부러 비워, 안쪽 공간이 왼쪽/오른쪽 두 방향 모두로
	// 통하게 했다. 그래야 bMirrorY로 뒤집어도 웨이포인트가 그 안쪽에 있을 때 입구가 하나뿐인 경우가
	// 생기지 않아, 체크포인트 이후 다른 쪽 입구로 되돌아 나올 수 있다(13레벨에서 겪었던 "단일 통로에
	// 웨이포인트가 갇히는" 버그와 동일한 유형을 미연에 방지).
	// =================================================================================

	// Level 16 (20x10): 4단계 시작 - 기본 방향의 모서리 장애물
	{
		TArray<FIntPoint> Walls, Traps;
		AddZoneCornerWalls(Walls, 0, false);   // 좌측 구역
		AddZoneCornerWalls(Walls, 10, false);  // 우측 구역
		AddLevel(16, 20, 10, FIntPoint(0, 0), FIntPoint(19, 9),
			{ FIntPoint(9, 3), FIntPoint(12, 3) }, Walls, Traps, {}, {}, {}, {}, 4,
			{ FIntPoint(8, 2), FIntPoint(5, 8), FIntPoint(15, 2), FIntPoint(15, 8) }, // 시작/구역 진입 지점에서 멀리 떨어진 안쪽
			{ true, true }, // 두 웨이포인트 모두 도착 시 카메라 이동 (아래 focus 좌표가 실제로 바라볼 지점)
			{ FIntPoint(12, 3), FIntPoint(17, 7) }); // 1번은 2번 웨이포인트 쪽을, 2번은 목표 타일이 화면에 들어올 만큼만 목표 쪽으로 이동 (2번 웨이포인트도 화면 안에 남도록 절충)
	}

	// Level 17 (20x10): 장애물을 세로로 뒤집어 배치
	{
		TArray<FIntPoint> Walls, Traps;
		AddZoneCornerWalls(Walls, 0, true);
		AddZoneCornerWalls(Walls, 10, true);
		AddLevel(17, 20, 10, FIntPoint(0, 0), FIntPoint(19, 9),
			{ FIntPoint(9, 3), FIntPoint(12, 3) }, Walls, Traps, {}, {}, {}, {}, 4,
			{ FIntPoint(8, 2), FIntPoint(5, 8), FIntPoint(15, 2), FIntPoint(15, 8) },
			{ true, true }, // 두 웨이포인트 모두 도착 시 카메라 이동 (아래 focus 좌표가 실제로 바라볼 지점)
			{ FIntPoint(12, 3), FIntPoint(17, 7) }); // 1번은 2번 웨이포인트 쪽을, 2번은 목표 타일이 화면에 들어올 만큼만 목표 쪽으로 이동 (2번 웨이포인트도 화면 안에 남도록 절충)
	}

	// Level 18 (20x10): 다시 기본 방향
	{
		TArray<FIntPoint> Walls, Traps;
		AddZoneCornerWalls(Walls, 0, false);
		AddZoneCornerWalls(Walls, 10, false);
		AddLevel(18, 20, 10, FIntPoint(0, 0), FIntPoint(19, 9),
			{ FIntPoint(9, 3), FIntPoint(12, 3) }, Walls, Traps, {}, {}, {}, {}, 4,
			{ FIntPoint(8, 2), FIntPoint(5, 8), FIntPoint(15, 2), FIntPoint(15, 8) },
			{ true, true }, // 두 웨이포인트 모두 도착 시 카메라 이동 (아래 focus 좌표가 실제로 바라볼 지점)
			{ FIntPoint(12, 3), FIntPoint(17, 7) }); // 1번은 2번 웨이포인트 쪽을, 2번은 목표 타일이 화면에 들어올 만큼만 목표 쪽으로 이동 (2번 웨이포인트도 화면 안에 남도록 절충)
	}

	// Level 19 (20x10): 다시 세로로 뒤집은 방향
	{
		TArray<FIntPoint> Walls, Traps;
		AddZoneCornerWalls(Walls, 0, true);
		AddZoneCornerWalls(Walls, 10, true);
		AddLevel(19, 20, 10, FIntPoint(0, 0), FIntPoint(19, 9),
			{ FIntPoint(9, 3), FIntPoint(12, 3) }, Walls, Traps, {}, {}, {}, {}, 4,
			{ FIntPoint(8, 2), FIntPoint(5, 8), FIntPoint(12, 1), FIntPoint(15, 8) }, // 2번 웨이포인트(12,3) 오른쪽에 있던 몬스터를 2칸 위(12,1)로 이동
			{ true, true }, // 두 웨이포인트 모두 도착 시 카메라 이동 (아래 focus 좌표가 실제로 바라볼 지점)
			{ FIntPoint(12, 3), FIntPoint(17, 7) }); // 1번은 2번 웨이포인트 쪽을, 2번은 목표 타일이 화면에 들어올 만큼만 목표 쪽으로 이동 (2번 웨이포인트도 화면 안에 남도록 절충)
	}

	// Level 20 (20x10): 4단계 마무리 - 좌측 구역은 기본 방향, 우측 구역은 세로로 뒤집어 좌우가 서로 다른
	// 비대칭 모양으로 만들고(16/18은 양쪽 다 기본, 19는 양쪽 다 반전이라 20만 유일한 조합), 몬스터
	// 한 마리를 목표 타일(19,9) 바로 옆(18,9)에 배치해 마무리 난이도를 준다.
	{
		TArray<FIntPoint> Walls, Traps;
		AddZoneCornerWalls(Walls, 0, false);
		AddZoneCornerWalls(Walls, 10, true);
		AddLevel(20, 20, 10, FIntPoint(0, 0), FIntPoint(19, 9),
			{ FIntPoint(9, 3), FIntPoint(12, 3) }, Walls, Traps, {}, {}, {}, {}, 4,
			{ FIntPoint(8, 2), FIntPoint(17, 8), FIntPoint(15, 2), FIntPoint(18, 9) }, // 좌측 하단에 있던 몬스터를 우측 하단 지역으로 이동, 마지막 몬스터는 목표 바로 옆에서 대기
			{ true, true }, // 두 웨이포인트 모두 도착 시 카메라 이동 (아래 focus 좌표가 실제로 바라볼 지점)
			{ FIntPoint(12, 3), FIntPoint(17, 7) }); // 1번은 2번 웨이포인트 쪽을, 2번은 목표 타일이 화면에 들어올 만큼만 목표 쪽으로 이동 (2번 웨이포인트도 화면 안에 남도록 절충)
	}

	// =================================================================================
	// 5단계 (Lv 21~24, 10x10): 새로운 함정 기믹 "순차 웨이브" 등장 - 지금까지의 위상차 교차 깜빡임(둘 중
	// 하나는 항상 열림)과 달리, 넓은 트랩 구역을 여러 그룹으로 나눠 "정확히 한 그룹만" 위험(Trap)해지고
	// 나머지는 전부 안전(Normal)한 상태를 순서대로 돌아가며 반복한다(SequentialWaveRows/Cols). 21/22는
	// 세로 방향(위→아래로 진행하며 좌우 폭 전체를 가로지르는 웨이브), 23/24는 21/22를 그대로 90도 돌린
	// 가로 방향(왼→오른쪽으로 진행하며 상하 폭 전체를 가로지르는 웨이브) 버전이다. 22/24는 웨이브 구역
	// 한가운데를 벽으로 나눠 안전한 "휴식 지점"을 만들어주는 대신, 웨이브 속도를 21/23보다 훨씬 빠르게
	// 해서 전체 난이도는 오히려 더 높였다.
	// =================================================================================

	// Level 21 (10x10): 순차 웨이브 세로 버전 - 왼쪽 통로(X0)에서 좌측 벽(X1, 4~5행만 뚫림)을 지나 폭
	// 6칸짜리 트랩 구역(X2~7)에 진입, 위에서부터 아래로(2줄씩 5개 그룹) 순서대로만 위험해지는 웨이브를
	// 뚫고 우측 벽(X8, 7~8행만 뚫림)으로 빠져나가면 목표. 그룹당 1초씩 위험, 한 주기 5초.
	{
		TArray<FIntPoint> Walls, Traps;
		TArray<ESATileTrapState> InitStates;
		TArray<float> NormalDurs, TrapDurs, InitialDelays;

		VWall(Walls, Traps, 1, 0, 9, { 4, 5 }, {});
		VWall(Walls, Traps, 8, 0, 9, { 7, 8 }, {});
		SequentialWaveRows(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 2, 7, 0, 9, 2, 1.0f);

		AddLevel(21, 10, 10, FIntPoint(0, 0), FIntPoint(9, 9),
			{}, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays);
	}

	// Level 22 (10x10): 21레벨과 같은 세로 웨이브 구조이지만, 트랩 구역 한가운데(X4~5)를 벽으로 갈라
	// (4,4)(4,5)(5,4)(5,5) 네 칸만 뚫어 놓은 "휴식 지점"을 만들었다 - 좌/우 트랩 블록(X2~3 / X6~7)을 각각
	// 통과하다가 이 안전지대에서 잠시 멈춰 다음 웨이브를 기다릴 수 있다. 대신 웨이브 속도를 그룹당 0.6초
	// (한 주기 3초)로 21레벨(그룹당 1초, 한 주기 5초)보다 훨씬 빠르게 만들어 난이도는 더 높였다.
	{
		TArray<FIntPoint> Walls, Traps;
		TArray<ESATileTrapState> InitStates;
		TArray<float> NormalDurs, TrapDurs, InitialDelays;

		VWall(Walls, Traps, 1, 0, 9, { 4, 5 }, {});
		VWall(Walls, Traps, 8, 0, 9, { 7, 8 }, {});
		VWall(Walls, Traps, 4, 0, 9, { 4, 5 }, {}); // 휴식 지점 좌측 벽
		VWall(Walls, Traps, 5, 0, 9, { 4, 5 }, {}); // 휴식 지점 우측 벽
		SequentialWaveRows(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 2, 3, 0, 9, 2, 0.6f); // 좌측 트랩 블록
		SequentialWaveRows(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 6, 7, 0, 9, 2, 0.6f); // 우측 트랩 블록

		AddLevel(22, 10, 10, FIntPoint(0, 0), FIntPoint(9, 9),
			{}, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays);
	}

	// Level 23 (10x10): 21레벨을 그대로 90도 회전시킨 가로 버전 - 위쪽 통로(Y0)에서 상단 벽(Y1, 4~5열만
	// 뚫림)을 지나 높이 6칸짜리 트랩 구역(Y2~7)에 진입, 왼쪽부터 오른쪽으로(2칸씩 5개 그룹) 순서대로만
	// 위험해지는 웨이브를 뚫고 하단 벽(Y8, 7~8열만 뚫림)으로 빠져나가면 목표. 그룹당 1초씩 위험, 한 주기 5초.
	{
		TArray<FIntPoint> Walls, Traps;
		TArray<ESATileTrapState> InitStates;
		TArray<float> NormalDurs, TrapDurs, InitialDelays;

		HWall(Walls, Traps, 1, 0, 9, { 4, 5 }, {});
		HWall(Walls, Traps, 8, 0, 9, { 7, 8 }, {});
		SequentialWaveCols(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 0, 9, 2, 7, 2, 1.0f);

		AddLevel(23, 10, 10, FIntPoint(0, 0), FIntPoint(9, 9),
			{}, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays);
	}

	// Level 24 (10x10): 22레벨을 그대로 90도 회전시킨 가로 버전 - 트랩 구역 한가운데(Y4~5)를 벽으로 갈라
	// (4,4)(4,5)(5,4)(5,5) 네 칸만 뚫어 놓은 휴식 지점을 만들었고, 위/아래 트랩 블록(Y2~3 / Y6~7)을 각각
	// 통과하다가 이곳에서 잠시 멈출 수 있다. 웨이브 속도는 22레벨과 동일하게 그룹당 0.6초(한 주기 3초)로 높였다.
	{
		TArray<FIntPoint> Walls, Traps;
		TArray<ESATileTrapState> InitStates;
		TArray<float> NormalDurs, TrapDurs, InitialDelays;

		HWall(Walls, Traps, 1, 0, 9, { 4, 5 }, {});
		HWall(Walls, Traps, 8, 0, 9, { 7, 8 }, {});
		HWall(Walls, Traps, 4, 0, 9, { 4, 5 }, {}); // 휴식 지점 상단 벽
		HWall(Walls, Traps, 5, 0, 9, { 4, 5 }, {}); // 휴식 지점 하단 벽
		SequentialWaveCols(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 0, 9, 2, 3, 2, 0.6f); // 상단 트랩 블록
		SequentialWaveCols(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 0, 9, 6, 7, 2, 0.6f); // 하단 트랩 블록

		AddLevel(24, 10, 10, FIntPoint(0, 0), FIntPoint(9, 9),
			{}, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays);
	}

	// =================================================================================
	// 6단계 (Lv 25~30, 20x10): 24레벨의 함정 기믹을 기반으로 레벨 16~20에서 쓰던 "카메라 이동 웨이포인트"를
	// 다시 적용해 20x10으로 넓혔다. MaxCameraDistanceReferenceLevel(레벨15, 10x10)보다 가로로 두 배 넓어
	// 고정 카메라로는 전체가 다 보이지 않으므로, 웨이포인트에 도착할 때마다 카메라가 다음 구간 쪽으로
	// 패닝하며(레벨16~20과 동일한 방식) 그 웨이포인트가 동시에 체크포인트 역할도 한다. 목표 타일은
	// 25/28(상단), 26/29(중단), 27/30(하단)으로 매 레벨 다른 높이에 두었고, 함정도 지금까지 만든 여러
	// 방식(4/6레벨의 단일 문, 9/10/13/15레벨의 위상 교차 2칸 문, 7레벨의 폭 전체 함정 띠, 21~24레벨의
	// 순차 웨이브)을 레벨마다 하나씩 번갈아 적용해 서로 다른 방식으로 발동되도록 했다. 시작/도착 지점
	// 사이의 좌우 통로(진입 전/구간 사이/탈출 후)는 항상 세로로 완전히 뚫려 있어, 목표 타일의 높이와
	// 무관하게 각 구간 진입 전에 자유롭게 위아래로 위치를 맞출 수 있다.
	// =================================================================================

	// Level 25 (20x10): 6단계 시작 - 4/6레벨과 같은 가장 단순한 "단일 트랩 문" 두 개를 연속 배치. 목표는 상단.
	{
		TArray<FIntPoint> Walls, Traps;

		VWall(Walls, Traps, 6, 0, 9, {}, { 3 });  // 첫 번째 문: (6,3) 한 칸만 트랩
		VWall(Walls, Traps, 13, 0, 9, {}, { 2 }); // 두 번째 문: (13,2) 한 칸만 트랩, 위상만 다르게

		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Trap, ESATileTrapState::Trap };
		TArray<float> NormalDurs = { 1.0f, 0.9f };
		TArray<float> TrapDurs = { 1.5f, 1.3f };
		TArray<float> InitialDelays = { 0.0f, 0.4f };

		AddLevel(25, 20, 10, FIntPoint(0, 0), FIntPoint(19, 1),
			{ FIntPoint(7, 3), FIntPoint(14, 2) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 0, {},
			{ true, true }, // 두 웨이포인트 모두 도착 시 카메라 이동
			{ FIntPoint(12, 2), FIntPoint(19, 1) }); // 1번은 2번 문 쪽을, 2번은 목표 쪽을 미리 보여준다
	}

	// Level 26 (20x10): 기존의 "벽 사이 2칸이 서로 바뀌며 깜빡이는" 단순한 문 대신, 4칸 중 "정확히 한 칸만"
	// 안전(Normal)하고 나머지 세 칸은 전부 위험(Trap)한 채로 그 안전한 한 칸이 위→아래로 이동하는 "회전
	// 안전칸" 문을 도입했다(21~24레벨 순차 웨이브와 반대로, 위험이 다수/안전이 소수). 배치도 앞 레벨들과
	// 다르게 첫 번째 문은 위쪽 4칸(Y1~4)에 두고, 두 번째 문은 아래쪽 4칸(Y5~8)에 두면서 안전칸이 아래→위로
	// (반대 방향) 더 빠르게 이동하도록 해서, 진짜 다른 두 종류의 문처럼 느껴지게 했다. 목표는 중단.
	{
		TArray<FIntPoint> Walls, Traps;

		VWall(Walls, Traps, 7, 0, 9, {}, { 1, 2, 3, 4 });  // 첫 번째 문: 위쪽 4칸 - 안전칸이 위(Y1)에서 아래(Y4)로 이동
		VWall(Walls, Traps, 14, 0, 9, {}, { 5, 6, 7, 8 }); // 두 번째 문: 아래쪽 4칸 - 안전칸이 아래(Y8)에서 위(Y5)로 이동 (반대 방향, 더 빠름)

		// Traps 배열 순서: [ (7,1),(7,2),(7,3),(7,4), (14,5),(14,6),(14,7),(14,8) ]
		TArray<ESATileTrapState> InitStates = { ESATileTrapState::Normal, ESATileTrapState::Normal, ESATileTrapState::Normal, ESATileTrapState::Normal,
			ESATileTrapState::Normal, ESATileTrapState::Normal, ESATileTrapState::Normal, ESATileTrapState::Normal };
		// 안전(Normal) 유지 시간은 짧게, 위험(Trap) 유지 시간은 "칸 수-1"배로 길게 줘서 항상 딱 한 칸만 안전해지도록 한다
		TArray<float> NormalDurs = { 0.4f, 0.4f, 0.4f, 0.4f, 0.3f, 0.3f, 0.3f, 0.3f };
		TArray<float> TrapDurs = { 1.2f, 1.2f, 1.2f, 1.2f, 0.9f, 0.9f, 0.9f, 0.9f };
		// 첫 번째 문(0.4초 주기): Y1→Y2→Y3→Y4 순서로 안전칸 이동. 두 번째 문(0.3초 주기): Y8→Y7→Y6→Y5 순서로 반대 이동
		TArray<float> InitialDelays = { 0.0f, 0.4f, 0.8f, 1.2f, 0.9f, 0.6f, 0.3f, 0.0f };

		AddLevel(26, 20, 10, FIntPoint(0, 0), FIntPoint(19, 4),
			{ FIntPoint(8, 2), FIntPoint(15, 6) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 0, {},
			{ true, true },
			{ FIntPoint(12, 2), FIntPoint(19, 4) }); // 1번은 2번 문 쪽을, 2번은 목표 쪽을 미리 보여준다
	}

	// Level 27 (20x10): 7레벨의 "폭 전체 함정 띠"(FullTrapRow)를 그리드 전체 가로 폭(20칸)으로 늘려 두 줄
	// 배치 - 벽 없이 오직 안전한 순간에 맞춰 줄 전체를 건너야 한다. 아래쪽 띠가 더 위험한 타이밍. 웨이포인트를
	// 4개(하단→상단→하단→상단)로 늘려 두 함정 띠(Y=3, Y=7)를 여러 번 오가도록 지그재그 경로를 강제한다.
	// 목표는 하단이라 마지막 구간에서 한 번 더 아래로 내려온다.
	{
		TArray<FIntPoint> Walls, Traps;
		TArray<ESATileTrapState> InitStates;
		TArray<float> NormalDurs, TrapDurs;

		FullTrapRow(Traps, 3, 0, 19);
		FullTrapRow(Traps, 7, 0, 19);
		for (int32 i = 0; i < 20; ++i) // 위쪽 함정 띠(Y=3)
		{
			InitStates.Add(ESATileTrapState::Normal);
			NormalDurs.Add(1.0f);
			TrapDurs.Add(1.0f);
		}
		for (int32 i = 0; i < 20; ++i) // 아래쪽 함정 띠(Y=7) - 안전 시간은 짧고 위험 시간은 김
		{
			InitStates.Add(ESATileTrapState::Normal);
			NormalDurs.Add(0.8f);
			TrapDurs.Add(1.2f);
		}

		AddLevel(27, 20, 10, FIntPoint(0, 0), FIntPoint(19, 8),
			{ FIntPoint(4, 8), FIntPoint(8, 1), FIntPoint(12, 8), FIntPoint(16, 1) }, // 하단→상단→하단→상단 순으로 왕복
			Walls, Traps, InitStates, NormalDurs, TrapDurs, {}, 0, {},
			{ true, true, true, true },
			{ FIntPoint(9, 4), FIntPoint(13, 4), FIntPoint(17, 4), FIntPoint(19, 4) }); // 포커스 Y를 항상 4로 고정해 그리드 세로 전체(0~9)가 계속 화면에 들어오게 하고, X만 각 웨이포인트에서 5칸 이내로 진행시킨다
	}

	// Level 28 (20x10): 21레벨의 세로 "순차 웨이브"를 8칸 폭(21레벨은 6칸)으로 더 넓혀 재등장. 목표는 상단.
	{
		TArray<FIntPoint> Walls, Traps;
		TArray<ESATileTrapState> InitStates;
		TArray<float> NormalDurs, TrapDurs, InitialDelays;

		VWall(Walls, Traps, 5, 0, 9, { 4, 5 }, {});  // 진입 통로 (5,4)/(5,5)만 뚫림
		SequentialWaveRows(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 6, 13, 0, 9, 2, 1.0f); // 8칸 폭 웨이브 (5개 그룹, 그룹당 1초)
		VWall(Walls, Traps, 14, 0, 9, { 7, 8 }, {}); // 탈출 통로 (14,7)/(14,8)만 뚫림

		AddLevel(28, 20, 10, FIntPoint(0, 0), FIntPoint(19, 1),
			{ FIntPoint(5, 4), FIntPoint(14, 7) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 0, {},
			{ true, true },
			{ FIntPoint(10, 7), FIntPoint(19, 4) }); // 두 웨이포인트 모두 자기 위치에서 5칸 이내로만 카메라를 옮겨, 도착 직후 캐릭터가 화면 밖으로 밀려나지 않도록 함
	}

	// Level 29 (20x10): 22레벨의 "가운데 휴식 지점이 있는 순차 웨이브"를 재등장시키되, 그 휴식 지점 자체를
	// 웨이포인트(체크포인트)로 삼았다 - 두 트랩 블록(6~7 / 10~11)을 각각 통과해 가운데(8~9)에서 쉬어갈 수
	// 있다. 웨이브 속도는 22레벨과 동일하게 그룹당 0.6초로 21레벨보다 빠르다. 목표는 중단.
	{
		TArray<FIntPoint> Walls, Traps;
		TArray<ESATileTrapState> InitStates;
		TArray<float> NormalDurs, TrapDurs, InitialDelays;

		VWall(Walls, Traps, 5, 0, 9, { 4, 5 }, {});   // 진입 통로
		SequentialWaveRows(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 6, 7, 0, 9, 2, 0.6f);   // 첫 번째 트랩 블록
		VWall(Walls, Traps, 8, 0, 9, { 4, 5 }, {});   // 휴식 지점 좌측 벽
		VWall(Walls, Traps, 9, 0, 9, { 4, 5 }, {});   // 휴식 지점 우측 벽
		SequentialWaveRows(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 10, 11, 0, 9, 2, 0.6f); // 두 번째 트랩 블록
		VWall(Walls, Traps, 12, 0, 9, { 7, 8 }, {});  // 탈출 통로

		AddLevel(29, 20, 10, FIntPoint(0, 0), FIntPoint(19, 4),
			{ FIntPoint(5, 4), FIntPoint(9, 4), FIntPoint(12, 7) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 2,
			{ FIntPoint(4, 6), FIntPoint(17, 2) }, // 입구(시작 타일)에서 4칸 떨어진 왼쪽 통로 / 두 번째 구간 입구에서 4칸 떨어진 오른쪽 통로에 한 마리씩
			{ true, true, true }, // 휴식 지점 도착 시에도 다음 구간이 보이도록 카메라 이동
			{ FIntPoint(9, 4), FIntPoint(13, 4), FIntPoint(17, 4) }); // 3번 웨이포인트 자신에서 5칸 이내로만 이동하도록 목표쪽 포커스를 (19,4)에서 (17,4)로 당김
	}

	// Level 30 (20x10): 6단계 마무리 - 26레벨의 위상 교차 2칸 문 두 개와 21/28레벨 계열의 순차 웨이브를
	// 한 레벨 안에 순서대로 결합한 종합 레벨. 웨이브도 그룹당 0.5초로 이번 단계에서 가장 빠르다. 목표는 하단.
	{
		TArray<FIntPoint> Walls, Traps;
		TArray<ESATileTrapState> InitStates;
		TArray<float> NormalDurs, TrapDurs, InitialDelays;

		// 첫 번째 위상 교차 2칸 문
		VWall(Walls, Traps, 5, 0, 9, {}, { 4, 5 });
		InitStates.Add(ESATileTrapState::Trap); InitStates.Add(ESATileTrapState::Trap);
		NormalDurs.Add(0.5f); NormalDurs.Add(0.5f);
		TrapDurs.Add(0.5f); TrapDurs.Add(0.5f);
		InitialDelays.Add(0.0f); InitialDelays.Add(0.5f);

		VWall(Walls, Traps, 7, 0, 9, { 4, 5 }, {});  // 웨이브 구간 진입 통로
		SequentialWaveRows(Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 8, 11, 0, 9, 2, 0.5f); // 가장 빠른 순차 웨이브
		VWall(Walls, Traps, 12, 0, 9, { 6, 7 }, {}); // 웨이브 구간 탈출 통로

		// 두 번째 위상 교차 2칸 문
		VWall(Walls, Traps, 15, 0, 9, {}, { 6, 7 });
		InitStates.Add(ESATileTrapState::Trap); InitStates.Add(ESATileTrapState::Trap);
		NormalDurs.Add(0.5f); NormalDurs.Add(0.5f);
		TrapDurs.Add(0.5f); TrapDurs.Add(0.5f);
		InitialDelays.Add(0.0f); InitialDelays.Add(0.5f);

		AddLevel(30, 20, 10, FIntPoint(0, 0), FIntPoint(19, 8),
			{ FIntPoint(7, 4), FIntPoint(12, 6), FIntPoint(16, 7) }, Walls, Traps, InitStates, NormalDurs, TrapDurs, InitialDelays, 2,
			{ FIntPoint(4, 6), FIntPoint(17, 3) }, // 입구에서 4칸 떨어진 왼쪽 통로 / 두 번째 문 통과 후 4칸 떨어진 오른쪽 통로에 한 마리씩
			{ true, true, true },
			{ FIntPoint(12, 6), FIntPoint(16, 7), FIntPoint(19, 8) });
	}
}
