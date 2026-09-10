// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SATypes.h"
#include "SAGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSAGameStateChanged, ESAGameStateType, NewState, ESAGameStateType, PrevState);

/**
 * 게임 진행 흐름(Menu, Game, Result)을 제어하고
 * Game 상태 시 SATileManager를 통해 레벨 타일을 생성하는 GameState 클래스
 */
UCLASS()
class SCRATCHANIMAL_API ASAGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASAGameState();

protected:
	virtual void BeginPlay() override;
	void OnAllActorsBeginPlayCompleted();

	/** 게임 초기화 완료 여부 플래그 (중복 호출 방지) */
	bool bHasInitializedGame;

public:
	// -------------------------------------------------------------
	// 설정 및 초기값
	// -------------------------------------------------------------

	/** 시작 시 진입할 초기 상태 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game State|Settings")
	ESAGameStateType InitialStateType;

	/** 시작 시 진입할 초기 레벨 번호 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game State|Settings")
	int32 InitialLevel;

	/** 목표 타일 도착 후 다음 레벨로 넘어가는 지연 시간 (초 단위, 0이면 즉시 전환) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game State|Settings")
	float NextLevelTransitionDelay;

	/** 이 배수 단위로 레벨을 클리어하면(예: 10, 20, 30...) 다음 레벨로 바로 넘어가지 않고 타일 와이프 연출 후 메뉴로 복귀 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game State|Settings", meta = (ClampMin = "1"))
	int32 MilestoneLevelInterval;

	// -------------------------------------------------------------
	// 상태 변수
	// -------------------------------------------------------------

	/** 현재 게임 상태 (Menu, Game, Result) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game State|State")
	ESAGameStateType CurrentStateType;

	/** 현재 진행 중인 레벨 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game State|State")
	int32 CurrentLevel;

	// -------------------------------------------------------------
	// 상태 제어 인터페이스
	// -------------------------------------------------------------

	/**
	 * 게임 상태 변경 및 관련 로직 실행
	 * @param NewState 전환할 게임 상태 (Menu, Game, Result)
	 * @param TargetLevel Game 상태 전환 시 로드할 레벨 번호 (기본값: 1)
	 */
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void SetGameStateType(ESAGameStateType NewState, int32 TargetLevel = 1);

	/** 지정된 레벨로 Game 상태 시작 */
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void StartGame(int32 LevelNumber = 1);

	/** 메뉴 상태로 전환 */
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void OpenMenu();

	/** 결과 화면 상태로 전환 (InResultType에 따라 WIN/LOSE 결과 화면이 표시됨) */
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void ShowResult(ESAResultType InResultType = ESAResultType::Lose);

	/** 현재 레벨 재시작 */
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void RestartLevel();

	/** 다음 레벨 시작 */
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void NextLevel();

	/** 현재 상태 반환 */
	UFUNCTION(BlueprintPure, Category = "Game State")
	ESAGameStateType GetCurrentStateType() const { return CurrentStateType; }

	/** 현재 레벨 번호 반환 */
	UFUNCTION(BlueprintPure, Category = "Game State")
	int32 GetCurrentLevel() const { return CurrentLevel; }

	// -------------------------------------------------------------
	// 이벤트 델리게이트
	// -------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Game State|Events")
	FOnSAGameStateChanged OnGameStateChanged;

protected:
	/** Menu 상태 진입 시 호출 */
	virtual void HandleMenuState();

	/** Game 상태 진입 시 호출 (SATileManager를 통한 타일 생성) */
	virtual void HandleGameState(int32 LevelNumber);

	/** Result 상태 진입 시 호출 */
	virtual void HandleResultState();

	/** SATileManager의 OnLevelCompleted 이벤트를 받아 다음 레벨을 로드하는 핸들러 */
	UFUNCTION()
	void OnTileLevelCompleted(int32 CompletedLevelNumber);

	/** SAMenuManager의 OnLevelSelected 이벤트를 받아 해당 레벨로 Game(Ingame) 상태를 시작하는 핸들러 */
	UFUNCTION()
	void OnMenuLevelSelected(int32 SelectedLevelNumber);

	/** SATileManager의 OnTrapTriggered 이벤트를 받아 레벨 이탈 연출(타일 와이프) 후 Result(LOSE)로 전환시키는 핸들러 */
	UFUNCTION()
	void HandleTrapTriggered();

	/** SATileManager의 OnMonsterCaughtPlayer 이벤트를 받아 레벨 이탈 연출(타일 와이프) 후 Result(LOSE)로 전환시키는 핸들러 */
	UFUNCTION()
	void HandleMonsterCaughtPlayer();

	/** SATileManager의 OnMenuButtonSwiped 이벤트를 받아 즉시 메뉴 상태로 전환하는 핸들러 */
	UFUNCTION()
	void HandleMenuButtonSwiped();

	/** SATileManager의 OnTileWipeCompleted 이벤트를 받아 PendingResultType에 따라 실제로 Result 상태로 전환하는 핸들러 */
	UFUNCTION()
	void HandleTileWipeCompleted();

	/** SAResultManager의 OnResultUnlockSwiped 이벤트를 받아 메뉴 상태로 전환하는 핸들러 */
	UFUNCTION()
	void HandleResultUnlockSwiped();

	/**
	 * Trap 발동이나 10단계 클리어 등으로 레벨을 이탈할 때 재생하는 공통 연출:
	 * 동물/음식 액터와 트랩 이펙트 풀을 전부 삭제하고, 타일이 바깥에서 안쪽으로 사라지는 와이프 연출을 시작한다.
	 * (연출이 끝나면 HandleTileWipeCompleted를 통해 InResultType에 맞는 Result 화면으로 전환됨)
	 */
	void PlayLevelExitWipeAndShowResult(ESAResultType InResultType);

	/** PlayLevelExitWipeAndShowResult로 예약해둔, 와이프 연출이 끝난 뒤 보여줄 결과 타입 */
	ESAResultType PendingResultType;
};
