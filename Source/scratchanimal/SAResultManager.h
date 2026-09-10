// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SATypes.h"
#include "SAResultTileActor.h"
#include "SAResultManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResultUnlockSwiped);

/**
 * 결과(Result) 화면을 SATileManager/SAMenuManager와 같은 방식(드래그 타일 그리드)으로 구현하기 위한 WorldSubsystem.
 * 배경 타일은 전혀 그리지 않고, WIN/LOSE 글자의 획(직선 스트로크)을 따라 촘촘히 찍은 점들만 타일로 스폰한다
 * (좌표는 정수 그리드에 맞출 필요 없이 자유로운 실수 값을 사용). 화면 우측 하단에는 아이폰 잠금 해제 같은
 * 1x2 드래그 슬라이더 타일 두 개만 별도로 배치되며, 그 슬라이더를 끝까지 드래그하면 OnResultUnlockSwiped가 발생한다.
 */
UCLASS()
class SCRATCHANIMAL_API USAResultManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	USAResultManager();

	virtual void Deinitialize() override;

	/** 어디서든 쉽게 접근할 수 있는 정적 헬퍼 함수 */
	UFUNCTION(BlueprintPure, Category = "Result Manager", meta = (WorldContext = "WorldContextObject"))
	static USAResultManager* Get(const UObject* WorldContextObject);

	// -------------------------------------------------------------
	// 설정 옵션
	// -------------------------------------------------------------

	/** 스폰할 결과 타일 액터 클래스 (기본값: ASAResultTileActor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Manager|Settings")
	TSubclassOf<ASAResultTileActor> ResultTileActorClass;

	/** 글자 배치 기준 원점 위치 (단어는 이 지점을 중심으로 가운데 정렬됨) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Manager|Settings")
	FVector GridOrigin;

	/** 문자 로컬 좌표 1칸이 실제 월드에서 몇 유닛인지 (작을수록 글자가 작아짐 - 기존 타일 간격 105보다 훨씬 작게 잡아야 화면에 다 들어옴) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Manager|Settings", meta = (ClampMin = "1.0"))
	float LetterUnitSpacing;

	/** 획(스트로크)을 따라 점을 찍는 간격 (문자 로컬 좌표 기준). 0.5면 대각선도 촘촘하게 찍혀 매끄러워 보임 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Manager|Settings", meta = (ClampMin = "0.05"))
	float StrokeSampleStep;

	/** 글자 사이 간격 (문자 로컬 좌표 기준, 문자 너비 4칸에 대한 여백) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Manager|Settings", meta = (ClampMin = "0.0"))
	float LetterGapUnits;

	/** 슬라이더(좌우 두 타일)의 중심 위치 - GridOrigin 기준 오프셋(월드 유닛). +X가 오른쪽, +Y가 아래쪽. 기본값은 글자 아래 가운데 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Manager|Settings")
	FVector2D SliderOffsetFromOrigin;

	/** 슬라이더 두 타일(왼쪽/오른쪽) 사이의 간격 (월드 유닛) - 가로로 드래그하는 슬라이더 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Manager|Settings", meta = (ClampMin = "1.0"))
	float SliderTileGap;

	/** WIN 글자 패턴 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Manager|Settings")
	FLinearColor WinPatternColor;

	/** LOSE 글자 패턴 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Manager|Settings")
	FLinearColor LosePatternColor;

	// -------------------------------------------------------------
	// 주요 기능 인터페이스
	// -------------------------------------------------------------

	/**
	 * 결과 화면을 생성한다. Win이면 "WIN", Lose면 "LOSE" 글자를 획(스트로크) 패턴을 따라 배치하고
	 * (배경 타일은 그리지 않음), 우측 하단에 1x2 드래그 슬라이더(잠금 해제용)를 배치한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Result Manager")
	void ShowResult(ESAResultType InResultType);

	/** 생성된 결과 타일을 모두 제거 */
	UFUNCTION(BlueprintCallable, Category = "Result Manager")
	void ClearResultTiles();

	// -------------------------------------------------------------
	// 터치 및 드래그 경로 처리 인터페이스 (SAPlayerController에서 호출)
	// -------------------------------------------------------------

	/** 터치 또는 마우스 누름 시 호출 (드래그 가능한 슬라이더 타일에서만 새 드래그가 시작됨) */
	UFUNCTION(BlueprintCallable, Category = "Result Manager|Interaction")
	void ProcessTileTouchBegin(ASAResultTileActor* TouchedTile);

	/** 터치 드래그 중 다른 타일 위로 이동했을 때 호출 */
	UFUNCTION(BlueprintCallable, Category = "Result Manager|Interaction")
	void ProcessTileTouchMove(ASAResultTileActor* TouchedTile);

	/** 터치 또는 마우스를 뗐을 때 호출. 슬라이더 타일 두 개를 모두 거쳐 드래그했다면 OnResultUnlockSwiped가 발생한다. */
	UFUNCTION(BlueprintCallable, Category = "Result Manager|Interaction")
	void ProcessTileTouchEnd();

	/** 두 타일이 상하좌우 4방향으로 인접해 있는지 확인 */
	UFUNCTION(BlueprintPure, Category = "Result Manager|Utility")
	static bool AreTilesAdjacent(const ASAResultTileActor* TileA, const ASAResultTileActor* TileB);

	/** 현재 드래그 중인지 여부 */
	UFUNCTION(BlueprintPure, Category = "Result Manager|State")
	bool IsDragging() const { return bIsDragging; }

	/** 문자 로컬 좌표(실수 가능)를 GridOrigin 기준 월드 위치로 계산하여 반환 */
	UFUNCTION(BlueprintPure, Category = "Result Manager")
	FVector CalculateLetterWorldLocation(float InLocalX, float InLocalY) const;

	// -------------------------------------------------------------
	// 이벤트 델리게이트
	// -------------------------------------------------------------

	/** 우측 하단 슬라이더를 끝까지 드래그해서 "잠금 해제"에 성공했을 때 발생 */
	UPROPERTY(BlueprintAssignable, Category = "Result Manager|Events")
	FOnResultUnlockSwiped OnResultUnlockSwiped;

private:
	/** 현재까지 스폰된 모든 결과 타일 (패턴 타일 + 슬라이더 타일) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASAResultTileActor>> SpawnedTiles;

	/** 현재 드래그 중인 경로 (슬라이더 타일만 포함될 수 있음) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASAResultTileActor>> SelectedPath;

	/** 현재 드래그 중 여부 */
	UPROPERTY(Transient)
	bool bIsDragging;

	/** 드래그 경로의 선택 하이라이트를 전부 해제하고 경로를 비움 */
	void ResetDragPath();

	/** 지정된 월드 위치에 결과 타일 하나를 스폰하고 상태(패턴 On/Off, 드래그 가능 여부, 그리드 좌표)를 설정 */
	ASAResultTileActor* SpawnResultTile(const FVector& InWorldLocation, bool bInPatternLit, bool bInDraggable, const FLinearColor& InPatternColor, const FIntPoint& InGridCoord);

	/**
	 * 지정된 단어를 획(스트로크) 단위로 정의된 글꼴을 이용해, GridOrigin을 중심으로 가운데 정렬된
	 * 문자 로컬 좌표(실수) 점들의 목록으로 변환한다. 각 점이 타일 하나에 대응한다.
	 */
	TArray<FVector2D> BuildLetterPattern(const FString& Word) const;
};
