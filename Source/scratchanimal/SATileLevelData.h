// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SATypes.h"
#include "SATileLevelData.generated.h"

/**
 * 개별 타일의 배치 정보 (좌표, 타입, 웨이포인트 순서 번호 등)
 */
USTRUCT(BlueprintType)
struct FSATileInfo
{
	GENERATED_BODY()

	/** 그리드 좌표 (X, Y) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	FIntPoint Coord = FIntPoint::ZeroValue;

	/** 타일의 종류 (시작, 벽, 웨이포인트, 함정, 목표, 일반) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	ESATileType TileType = ESATileType::Normal;

	/** 웨이포인트 순서 번호 (1, 2, 3...) - 웨이포인트 타일일 경우 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	int32 WaypointIndex = 0;

	FSATileInfo() = default;

	FSATileInfo(const FIntPoint& InCoord, ESATileType InType, int32 InWaypointIndex = 0)
		: Coord(InCoord), TileType(InType), WaypointIndex(InWaypointIndex)
	{
	}
};

/**
 * 특정 레벨(스테이지)의 전체 타일 규격 및 배치 데이터
 */
USTRUCT(BlueprintType)
struct FSATileLevelData
{
	GENERATED_BODY()

	/** 레벨 번호 (1 ~ 50) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 LevelNumber = 1;

	/** 그리드 가로 타일 수 (열 수, Width) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level", meta = (ClampMin = "3"))
	int32 GridWidth = 5;

	/** 그리드 세로 타일 수 (행 수, Height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level", meta = (ClampMin = "3"))
	int32 GridHeight = 5;

	/** 시작 타일 좌표 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Special Tiles")
	FIntPoint StartCoord = FIntPoint(0, 0);

	/** 목표 타일 좌표 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Special Tiles")
	FIntPoint GoalCoord = FIntPoint(4, 4);

	/** 순서대로 경유해야 하는 웨이포인트 타일 좌표 목록 (Index 0 = 1번 웨이포인트, Index 1 = 2번 ...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Special Tiles")
	TArray<FIntPoint> WaypointCoords;

	/**
	 * 웨이포인트별로 도착 시 카메라를 이동시킬지 여부 - WaypointCoords와 동일한 인덱스로 매칭됨.
	 * 지정하지 않으면 false(카메라 이동 없음). 레벨 16~20에서 사용할 카메라 이동 기믹용 (ASATileActor::bMoveCameraOnArrival 참고)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Special Tiles")
	TArray<bool> WaypointMovesCamera;

	/**
	 * 웨이포인트별로, 카메라가 실제로 이동해서 바라볼 그리드 좌표 - WaypointCoords와 동일한 인덱스로 매칭됨.
	 * 지정하지 않으면 그 웨이포인트 자신의 좌표를 그대로 사용한다. 다음 웨이포인트나 목표 타일 좌표를
	 * 지정해두면, 도착 즉시 "다음에 갈 곳"이 화면에 미리 보이도록 카메라를 그쪽으로 옮길 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Special Tiles")
	TArray<FIntPoint> WaypointCameraFocusCoords;

	/** 이 레벨에 등장하는 SAMonsterActor(몬스터)의 마리 수. 0이면 몬스터가 등장하지 않는다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Monster", meta = (ClampMin = "0"))
	int32 MonsterCount = 0;

	/**
	 * 몬스터별 지정 스폰 좌표 (Index 0 = 1번째 몬스터, Index 1 = 2번째 몬스터...). 비어있으면 각 몬스터가
	 * 스스로 시작 타일에서 가장 멀리 떨어진 이동 가능 타일을 찾아 스폰한다 (ASAMonsterActor::FindSpawnTile 참고).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Monster")
	TArray<FIntPoint> MonsterSpawnCoords;

	/** 이동 불가 벽 타일 좌표 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Obstacles")
	TArray<FIntPoint> WallCoords;

	/** 함정(방해) 타일 좌표 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Obstacles")
	TArray<FIntPoint> TrapCoords;

	/** 함정 타일별 초기 상태 오버라이드 목록 - TrapCoords와 동일한 인덱스로 매칭됨. 지정하지 않으면 액터 기본값을 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Obstacles")
	TArray<ESATileTrapState> TrapInitialStates;

	/** 함정 타일별 Normal 상태 유지 시간(초) 오버라이드 목록 - TrapCoords와 동일한 인덱스로 매칭됨. 지정하지 않으면 액터 기본값을 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Obstacles")
	TArray<float> TrapNormalDurations;

	/** 함정 타일별 Trap 상태 유지 시간(초) 오버라이드 목록 - TrapCoords와 동일한 인덱스로 매칭됨. 지정하지 않으면 액터 기본값을 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Obstacles")
	TArray<float> TrapDurations;

	/**
	 * 함정 타일별 최초 대기 시간(초) 오버라이드 목록 - TrapCoords와 동일한 인덱스로 매칭됨. 지정하지 않으면 0(대기 없음).
	 * 같은 주기의 다른 함정 타일들과 이 값을 다르게 주면 위상이 어긋난 "교차" 패턴을 만들 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level|Obstacles", meta = (ClampMin = "0.0"))
	TArray<float> TrapInitialDelays;

	/** 웨이포인트 좌표에 대응하는 "도착 시 카메라 이동" 설정을 반환 (지정되지 않았으면 false) */
	bool DoesWaypointMoveCamera(const FIntPoint& InCoord) const
	{
		const int32 FoundIdx = WaypointCoords.IndexOfByKey(InCoord);
		if (FoundIdx != INDEX_NONE && WaypointMovesCamera.IsValidIndex(FoundIdx))
		{
			return WaypointMovesCamera[FoundIdx];
		}
		return false;
	}

	/** 웨이포인트 좌표에 대응하는 카메라 포커스 좌표를 반환 (지정되지 않았으면 그 웨이포인트 자신의 좌표를 그대로 반환) */
	FIntPoint GetWaypointCameraFocusCoord(const FIntPoint& InCoord) const
	{
		const int32 FoundIdx = WaypointCoords.IndexOfByKey(InCoord);
		if (FoundIdx != INDEX_NONE && WaypointCameraFocusCoords.IsValidIndex(FoundIdx))
		{
			return WaypointCameraFocusCoords[FoundIdx];
		}
		return InCoord;
	}

	/** 함정 타일 좌표에 대응하는 최초 대기 시간을 반환 (지정되지 않았으면 0) */
	float GetTrapInitialDelay(const FIntPoint& InCoord) const
	{
		const int32 FoundIdx = TrapCoords.IndexOfByKey(InCoord);
		if (FoundIdx != INDEX_NONE && TrapInitialDelays.IsValidIndex(FoundIdx))
		{
			return TrapInitialDelays[FoundIdx];
		}
		return 0.0f;
	}

	/** 특정 함정 좌표에 대한 사이클 오버라이드(초기 상태, Normal/Trap 유지 시간)를 찾는다. 세 배열 모두에 값이 지정되어 있어야 true를 반환 */
	bool GetTrapCycleOverride(const FIntPoint& InCoord, ESATileTrapState& OutInitialState, float& OutNormalDuration, float& OutTrapDuration) const
	{
		const int32 FoundIdx = TrapCoords.IndexOfByKey(InCoord);
		if (FoundIdx == INDEX_NONE
			|| !TrapInitialStates.IsValidIndex(FoundIdx)
			|| !TrapNormalDurations.IsValidIndex(FoundIdx)
			|| !TrapDurations.IsValidIndex(FoundIdx))
		{
			return false;
		}

		OutInitialState = TrapInitialStates[FoundIdx];
		OutNormalDuration = TrapNormalDurations[FoundIdx];
		OutTrapDuration = TrapDurations[FoundIdx];
		return true;
	}

	/** 특정 좌표의 타일 종류와 웨이포인트 번호를 반환하는 편의 함수 */
	ESATileType GetTileTypeAt(const FIntPoint& InCoord, int32& OutWaypointIndex) const
	{
		OutWaypointIndex = 0;

		if (InCoord == StartCoord)
		{
			return ESATileType::Start;
		}

		if (InCoord == GoalCoord)
		{
			return ESATileType::Goal;
		}

		const int32 FoundIdx = WaypointCoords.IndexOfByKey(InCoord);
		if (FoundIdx != INDEX_NONE)
		{
			OutWaypointIndex = FoundIdx + 1; // 1-based index (1, 2, 3...)
			return ESATileType::Waypoint;
		}

		if (WallCoords.Contains(InCoord))
		{
			return ESATileType::Wall;
		}

		if (TrapCoords.Contains(InCoord))
		{
			return ESATileType::Trap;
		}

		return ESATileType::Normal;
	}

	/** 좌표가 유효한 그리드 범위 내인지 확인 */
	bool IsValidCoord(const FIntPoint& InCoord) const
	{
		return InCoord.X >= 0 && InCoord.X < GridWidth && InCoord.Y >= 0 && InCoord.Y < GridHeight;
	}
};
