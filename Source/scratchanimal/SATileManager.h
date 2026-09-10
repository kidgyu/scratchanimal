// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SATypes.h"
#include "SATileActor.h"
#include "SATileLevelData.h"
#include "SATileManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelLoaded, int32, LevelNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTilesCleared);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaypointReached, int32, WaypointIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTrapTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelCompleted, int32, LevelNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPathReset);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTileWipeCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMonsterCaughtPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuButtonSwiped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaypointCameraMoveRequested, FVector, TargetLocation);

/**
 * 월드 내 타일의 생성, 배치, 검색, 드래그 경로 검증 및 레벨 전환을 총괄하는 WorldSubsystem
 */
UCLASS()
class SCRATCHANIMAL_API USATileManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	USATileManager();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 어디서든 쉽게 접근할 수 있는 정적 헬퍼 함수 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager", meta = (WorldContext = "WorldContextObject"))
	static USATileManager* Get(const UObject* WorldContextObject);

	// -------------------------------------------------------------
	// 설정 옵션
	// -------------------------------------------------------------

	/** 스폰할 타일 액터 클래스 (기본값: ASATileActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Manager|Settings")
	TSubclassOf<ASATileActor> TileActorClass;

	/** 타일 간 중심 간격 (기본 105cm: 타일 크기 100cm + 5cm 여백) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Manager|Settings")
	float TileStep;

	/** 그리드 생성 기준 원점 위치 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Manager|Settings")
	FVector GridOrigin;

	/** 그리드를 GridOrigin을 중심으로 대칭 정렬할지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Manager|Settings")
	bool bCenterGrid;

	// -------------------------------------------------------------
	// 나가기(메뉴 이동) 슬라이더 (화면 우상단, ASAPlayerPawn에 붙어 항상 같은 화면 위치를 유지)
	// -------------------------------------------------------------

	/**
	 * 나가기 슬라이더를 그리드 표면(폰의 현재 Z)보다 얼마나 위에 배치할지 (월드 유닛). ASATileActor와
	 * 같은 높이에 두면 기울어진 카메라 시점에서 타일과 겹쳐 보일 수 있으므로 충분히 띄워준다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Manager|Menu Button")
	float MenuButtonHeightOffset;

	/** 나가기 슬라이더 위/아래 두 타일 사이의 간격 (월드 유닛) - 아래쪽 타일은 평소 숨겨져 있다가 위쪽 타일을 터치하면 드러난다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Manager|Menu Button", meta = (ClampMin = "1.0"))
	float MenuButtonTileGap;

	/** 나가기 슬라이더 타일의 기본(선택 전) 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Manager|Menu Button")
	FLinearColor MenuButtonColor;

	// -------------------------------------------------------------
	// 상태 변수
	// -------------------------------------------------------------

	/** 현재 로드된 레벨 번호 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Manager|State")
	int32 CurrentLevelNumber;

	/** 현재 로드된 레벨의 데이터 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Manager|State")
	FSATileLevelData CurrentLevelData;

	// -------------------------------------------------------------
	// 주요 기능 인터페이스
	// -------------------------------------------------------------

	/**
	 * SATileSettings에서 해당 레벨 데이터를 읽어와 월드에 타일들을 실제로 생성
	 * @param InLevelNumber 로드할 레벨 번호 (1 ~ 50)
	 * @return 레벨 로드 및 타일 생성 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager")
	bool LoadLevel(int32 InLevelNumber);

	/** 현재 레벨을 처음 상태로 다시 로드 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager")
	bool RestartCurrentLevel();

	/** 다음 레벨 로드 (현재 레벨 + 1) */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager")
	bool LoadNextLevel();

	/** 월드에 생성된 모든 타일 액터를 제거하고 관리 맵 비우기 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager")
	void ClearTiles();

	/** 모든 타일의 드래그 선택 상태(빨간색)를 초기화 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager")
	void ResetAllTilesSelection();

	// -------------------------------------------------------------
	// 터치 및 드래그 경로 처리 인터페이스 (SAPlayerController에서 호출)
	// -------------------------------------------------------------

	/** 터치 또는 마우스 누름 시 호출 (새로운 드래그 시작 또는 이어가기) */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager|Interaction")
	void ProcessTileTouchBegin(ASATileActor* TouchedTile);

	/** 터치 드래그 중 다른 타일 위로 이동했을 때 호출 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager|Interaction")
	void ProcessTileTouchMove(ASATileActor* TouchedTile);

	/** 터치 또는 마우스 뗐을 때 호출 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager|Interaction")
	void ProcessTileTouchEnd();

	/** 현재 선택된 타일 경로 초기화 (bResetCheckpoints가 true면 저장된 웨이포인트 경로도 리셋) */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager|Interaction")
	void ResetDragPath(bool bResetCheckpoints = true);

	/**
	 * Trap 타일이 Normal → Trap으로 전환되는 순간, 그 타일이 지금 드래그 경로의 "마지막(현재 위치)"라면
	 * 방금 함정을 밟은 것과 동일하게 경로 전체를 초기화한다. 이미 지나온 중간 경로 타일이라면 무시한다
	 * (ASATileActor::ApplyTrapVisualState에서 호출).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager|Interaction")
	void NotifyTileBecameTrapWhileHeld(ASATileActor* Tile);

	/** 몬스터 액터가 플레이어가 있는 타일에 도달했을 때 호출 (ASAMonsterActor에서 호출) - OnMonsterCaughtPlayer를 발생시킨다 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager|Interaction")
	void NotifyMonsterCaughtPlayer();

	/**
	 * 카메라 이동 웨이포인트(ASATileActor::bMoveCameraOnArrival)에 도착해 터치 입력을 잠갔을 때, 카메라가
	 * 목표 지점까지 이동을 완료한 뒤(ASAPlayerPawn) 호출되어 다시 터치할 수 있는 상태로 되돌린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager|Interaction")
	void NotifyCameraMoveCompleted();

	/** 두 타일이 상하좌우 4방향으로 인접해 있는지 확인 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager|Utility")
	static bool AreTilesAdjacent(const ASATileActor* TileA, const ASATileActor* TileB);

	/**
	 * 화면 우상단 나가기 슬라이더 두 타일(위/아래)을 플레이어 폰(ASAPlayerPawn)에 부착해 (재)스폰한다.
	 * 카메라 거리/위치가 확정된 뒤에 호출해야 정확한 화면 위치가 나오므로, ASAPlayerPawn::FocusOnTileGrid에서
	 * 카메라 포커스를 마친 직후 호출한다 (인게임 상태일 때만).
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager|Menu Button")
	void SpawnMenuButtonTiles();

	// -------------------------------------------------------------
	// 타일 와이프 연출 (Trap 발동 / 10단계 클리어 등 레벨 이탈 연출용)
	// -------------------------------------------------------------

	/**
	 * 그리드 중심에서 먼 타일부터(바깥→안쪽) 시간차를 두고 파괴하는 연출을 시작한다.
	 * 진행 중에는 타일 터치 입력이 무시되며(bInputLocked), 모든 타일이 파괴되면 OnTileWipeCompleted가 발생한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile Manager|Wipe Effect")
	void StartTileWipeEffect();

	/** 와이프 한 단계(Step)마다 파괴할 타일 개수 대신, 전체 연출이 대략 이 시간(초) 안에 끝나도록 스텝당 개수를 자동 계산 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Manager|Wipe Effect", meta = (ClampMin = "0.1"))
	float WipeDuration;

	/** 와이프 각 단계(Step) 사이의 간격(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Manager|Wipe Effect", meta = (ClampMin = "0.01"))
	float WipeStepInterval;

	/** 현재 타일 와이프 연출이 진행 중인지(=입력이 잠겨있는지) 여부 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager|Wipe Effect")
	bool IsInputLocked() const { return bInputLocked; }

	/** 모든 타일이 와이프되어 파괴 완료되면 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnTileWipeCompleted OnTileWipeCompleted;

	/** 현재 드래그 중인지 여부 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager|State")
	bool IsDragging() const { return bIsDragging; }

	/** 현재 레벨 클리어 여부 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager|State")
	bool IsLevelCompleted() const { return bLevelCompleted; }

	/** 다음에 도달해야 할 웨이포인트 번호 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager|State")
	int32 GetNextWaypointIndexToVisit() const { return NextWaypointIndexToVisit; }

	/** 현재 선택된 타일 경로 목록 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager|State")
	TArray<ASATileActor*> GetSelectedPath() const;

	// -------------------------------------------------------------
	// 검색 및 조회 편의 함수
	// -------------------------------------------------------------

	/** 격자 좌표(Coord)에 위치한 타일 액터 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager")
	ASATileActor* GetTileAtCoord(const FIntPoint& InCoord) const;

	/** 격자 좌표(X, Y)에 위치한 타일 액터 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager")
	ASATileActor* GetTileAt(int32 InX, int32 InY) const;

	/** 현재 레벨의 시작 타일 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager")
	ASATileActor* GetStartTile() const { return StartTile; }

	/** 현재 레벨의 목표 타일 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager")
	ASATileActor* GetGoalTile() const { return GoalTile; }

	/** 특정 순서 번호의 웨이포인트 타일 반환 (1, 2, 3...) */
	UFUNCTION(BlueprintPure, Category = "Tile Manager")
	ASATileActor* GetWaypointTile(int32 InWaypointIndex) const;

	/** 스폰된 모든 타일 액터 목록 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager")
	TArray<ASATileActor*> GetAllTiles() const;

	/** 그리드 좌표를 월드 위치로 계산하여 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager")
	FVector CalculateTileWorldLocation(int32 InX, int32 InY) const;

	/** 현재 로드된 그리드의 중심 월드 위치 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager")
	FVector GetGridCenterLocation() const;

	/** 현재 로드된 그리드의 전체 가로/세로 월드 크기(cm) 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Manager")
	FVector2D GetGridWorldSize() const;

	// -------------------------------------------------------------
	// 이벤트 델리게이트
	// -------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnLevelLoaded OnLevelLoaded;

	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnTilesCleared OnTilesCleared;

	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnWaypointReached OnWaypointReached;

	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnTrapTriggered OnTrapTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnLevelCompleted OnLevelCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnPathReset OnPathReset;

	/** 몬스터 액터가 플레이어가 있는 타일에 도달(포획)했을 때 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnMonsterCaughtPlayer OnMonsterCaughtPlayer;

	/** 화면 우상단 메뉴 이동 슬라이더를 끝까지 드래그했을 때 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnMenuButtonSwiped OnMenuButtonSwiped;

	/**
	 * 카메라 이동 웨이포인트(ASATileActor::bMoveCameraOnArrival)에 도착했을 때 발생. 파라미터는 카메라가
	 * 이동해야 할 목표 월드 위치(해당 웨이포인트 타일 위치)이며, 발생과 동시에 터치 입력이 잠긴다
	 * (ASAPlayerPawn이 카메라 이동을 완료하면 NotifyCameraMoveCompleted를 호출해 다시 풀어준다).
	 */
	UPROPERTY(BlueprintAssignable, Category = "Tile Manager|Events")
	FOnWaypointCameraMoveRequested OnWaypointCameraMoveRequested;

private:
	/** 좌표별 스폰된 타일 매핑 */
	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<ASATileActor>> SpawnedTileMap;

	/** 웨이포인트 인덱스별(1, 2, 3...) 타일 매핑 */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<ASATileActor>> WaypointTileMap;

	/** 시작 타일 캐시 */
	UPROPERTY(Transient)
	TObjectPtr<ASATileActor> StartTile;

	/** 목표 타일 캐시 */
	UPROPERTY(Transient)
	TObjectPtr<ASATileActor> GoalTile;

	// -------------------------------------------------------------
	// 경로 진행 상태
	// -------------------------------------------------------------

	/** 현재 선택되어 있는 타일 경로 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASATileActor>> SelectedPath;

	/** 마지막으로 통과한 웨이포인트까지의 저장된 확정 경로 (체크포인트) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASATileActor>> SavedCheckpointPath;

	/** 다음에 방문해야 하는 웨이포인트 번호 (1-based) */
	UPROPERTY(Transient)
	int32 NextWaypointIndexToVisit;

	/** 체크포인트 기준 다음 방문 웨이포인트 번호 */
	UPROPERTY(Transient)
	int32 SavedWaypointIndex;

	/** 현재 드래그 중 여부 */
	UPROPERTY(Transient)
	bool bIsDragging;

	/** 현재 레벨 클리어 여부 */
	UPROPERTY(Transient)
	bool bLevelCompleted;

	/** 함정을 밟았을 때의 공통 처리 (경로 초기화, 이벤트 브로드캐스트, SFX 재생) */
	void TriggerTrapHit();

	// -------------------------------------------------------------
	// 타일 와이프 연출 상태
	// -------------------------------------------------------------

	/** 타일 와이프 연출 진행 중이거나 그 외 사유로 터치 입력을 무시해야 하는지 여부 */
	UPROPERTY(Transient)
	bool bInputLocked;

	/** 그리드 중심에서 먼 순서대로 정렬된, 아직 파괴되지 않은 와이프 대상 타일 목록 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASATileActor>> WipeOrderedTiles;

	/** 다음에 파괴할 WipeOrderedTiles의 시작 인덱스 */
	int32 WipeIndex;

	/** 와이프 한 스텝(Step)마다 파괴할 타일 개수 (StartTileWipeEffect에서 WipeDuration 기준으로 자동 계산됨) */
	int32 WipeTilesPerStep;

	/** 와이프 스텝 반복 타이머 */
	FTimerHandle WipeTimerHandle;

	/** 와이프 한 스텝을 진행 (타일 일부 파괴, 다 끝나면 정리하고 OnTileWipeCompleted 브로드캐스트) */
	void ProcessWipeStep();

	// -------------------------------------------------------------
	// 나가기(메뉴 이동) 슬라이더 상태
	// -------------------------------------------------------------

	/** 나가기 슬라이더 위쪽 타일 (평소 유일하게 보이는 타일) */
	UPROPERTY(Transient)
	TObjectPtr<ASATileActor> MenuButtonTileTop;

	/** 나가기 슬라이더 아래쪽 타일 (평소 숨김/충돌 비활성 상태, 위쪽 타일을 터치하면 드러남) */
	UPROPERTY(Transient)
	TObjectPtr<ASATileActor> MenuButtonTileBottom;

	/** 나가기 슬라이더를 현재 드래그 중인지 여부 (레벨 경로의 bIsDragging과는 독립적) */
	UPROPERTY(Transient)
	bool bIsMenuButtonDragging;

	/** 나가기 슬라이더의 현재 드래그 경로 (위쪽 -> 아래쪽 순서로만 채워짐) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASATileActor>> MenuButtonSelectedPath;

	/** 메뉴 이동 슬라이더 타일을 제거하고 드래그 상태를 초기화 (ClearTiles에서 호출) */
	void DestroyMenuButtonTiles();
};
