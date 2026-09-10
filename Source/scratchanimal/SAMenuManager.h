// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SAMenuTileActor.h"
#include "SAMenuManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuLevelSelected, int32, LevelNumber);

/**
 * 레벨 선택 메뉴 화면을 SATileManager/SATileActor와 같은 방식(드래그 타일 그리드)으로 구현하기 위한 WorldSubsystem.
 * 시작 타일은 레벨 그리드보다 한 칸 왼쪽(그리드 좌표 (-1,0))에 예외적으로 배치되고, 레벨 그리드(Y = 0..GridHeight-1)는
 * 한 줄(행)당 정확히 GridWidth개씩 레벨 번호가 순서대로 채워진다. 시작 타일에서 드래그를 시작해 마지막으로 놓은 타일에
 * 등록된 레벨 번호가 있으면 OnLevelSelected를 발생시키고, 없으면 경로 전체가 빈 타일 상태로 되돌아간다.
 */
UCLASS()
class SCRATCHANIMAL_API USAMenuManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	USAMenuManager();

	virtual void Deinitialize() override;

	/** 어디서든 쉽게 접근할 수 있는 정적 헬퍼 함수 */
	UFUNCTION(BlueprintPure, Category = "Menu Manager", meta = (WorldContext = "WorldContextObject"))
	static USAMenuManager* Get(const UObject* WorldContextObject);

	// -------------------------------------------------------------
	// 설정 옵션
	// -------------------------------------------------------------

	/** 스폰할 메뉴 타일 액터 클래스 (기본값: ASAMenuTileActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Manager|Settings")
	TSubclassOf<ASAMenuTileActor> MenuTileActorClass;

	/** 메뉴 그리드 가로 타일 수 (열 수) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Manager|Settings", meta = (ClampMin = "1"))
	int32 GridWidth;

	/** 메뉴 그리드 세로 타일 수 (행 수) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Manager|Settings", meta = (ClampMin = "1"))
	int32 GridHeight;

	/** 타일 간 중심 간격 (기본 105cm: 타일 크기 100cm + 5cm 여백) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Manager|Settings")
	float TileStep;

	/** 그리드 생성 기준 원점 위치 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Manager|Settings")
	FVector GridOrigin;

	/** 그리드를 GridOrigin을 중심으로 대칭 정렬할지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Manager|Settings")
	bool bCenterGrid;

	/**
	 * 레벨 그리드 한 줄(행)마다 순서대로 돌려쓰는 파스텔 톤 무지개색 목록.
	 * 예) 0번째 줄(레벨 1~GridWidth)은 0번 색, 1번째 줄(레벨 GridWidth+1~GridWidth*2)은 1번 색... 색상 개수를 넘어가면 다시 처음부터 순환.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Manager|Settings")
	TArray<FLinearColor> RowGroupColors;

	// -------------------------------------------------------------
	// 주요 기능 인터페이스
	// -------------------------------------------------------------

	/**
	 * 메뉴 타일 그리드를 생성한다. 시작 타일은 (-1,0)에 예외적으로 배치되고,
	 * 레벨 그리드(Y = 0..GridHeight-1)는 한 줄당 GridWidth개씩 앞에서부터 순서대로
	 * USATileSettings에 등록된 레벨 번호(1..총 레벨 수)를 배정하며, 남는 칸은 빈 타일로 둔다.
	 * 각 줄(행)은 RowGroupColors를 순환하며 서로 다른 파스텔 색으로 그룹화되어 표시된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Menu Manager")
	void LoadMenu();

	/** 생성된 메뉴 타일을 모두 제거 */
	UFUNCTION(BlueprintCallable, Category = "Menu Manager")
	void ClearMenuTiles();

	// -------------------------------------------------------------
	// 터치 및 드래그 경로 처리 인터페이스 (SAPlayerController에서 호출)
	// -------------------------------------------------------------

	/** 터치 또는 마우스 누름 시 호출 (시작 타일에서만 새 드래그가 시작됨) */
	UFUNCTION(BlueprintCallable, Category = "Menu Manager|Interaction")
	void ProcessTileTouchBegin(ASAMenuTileActor* TouchedTile);

	/** 터치 드래그 중 다른 타일 위로 이동했을 때 호출 */
	UFUNCTION(BlueprintCallable, Category = "Menu Manager|Interaction")
	void ProcessTileTouchMove(ASAMenuTileActor* TouchedTile);

	/**
	 * 터치 또는 마우스를 뗐을 때 호출. 마지막으로 드래그된 타일에 등록된 레벨이 있으면
	 * OnLevelSelected를 발생시키고, 없으면(또는 드래그하지 않았으면) 아무 일도 일어나지 않는다.
	 * 성공/실패와 관계없이 드래그 경로의 선택 하이라이트는 항상 초기화된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Menu Manager|Interaction")
	void ProcessTileTouchEnd();

	/** 두 타일이 상하좌우 4방향으로 인접해 있는지 확인 */
	UFUNCTION(BlueprintPure, Category = "Menu Manager|Utility")
	static bool AreTilesAdjacent(const ASAMenuTileActor* TileA, const ASAMenuTileActor* TileB);

	/** 현재 드래그 중인지 여부 */
	UFUNCTION(BlueprintPure, Category = "Menu Manager|State")
	bool IsDragging() const { return bIsDragging; }

	/** 격자 좌표에 위치한 메뉴 타일 액터 반환 */
	UFUNCTION(BlueprintPure, Category = "Menu Manager")
	ASAMenuTileActor* GetTileAtCoord(const FIntPoint& InCoord) const;

	/** 그리드 좌표를 월드 위치로 계산하여 반환 */
	UFUNCTION(BlueprintPure, Category = "Menu Manager")
	FVector CalculateTileWorldLocation(int32 InX, int32 InY) const;

	// -------------------------------------------------------------
	// 이벤트 델리게이트
	// -------------------------------------------------------------

	/** 유효한(레벨이 등록된) 타일에서 드래그를 놓았을 때 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Menu Manager|Events")
	FOnMenuLevelSelected OnLevelSelected;

private:
	/** 좌표별 스폰된 메뉴 타일 매핑 */
	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<ASAMenuTileActor>> SpawnedTileMap;

	/** 현재 드래그 중인 경로 (Index 0 = 시작 타일) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASAMenuTileActor>> SelectedPath;

	/** 현재 드래그 중 여부 */
	UPROPERTY(Transient)
	bool bIsDragging;

	/** 드래그 경로의 선택 하이라이트를 전부 해제하고 경로를 비움 */
	void ResetDragPath();

	/** 지정된 그리드 좌표에 메뉴 타일 하나를 스폰하고 상태(시작 여부/레벨 번호/줄 색상)를 설정 */
	ASAMenuTileActor* SpawnMenuTile(int32 InX, int32 InY, bool bIsStart, int32 InLevelNumber);
};
