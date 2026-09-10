// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SAResultTileActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * 결과(Result) 화면을 SATileActor/SAMenuTileActor와 같은 방식(드래그 타일 그리드)으로 구현하기 위한 타일 액터.
 * 대부분의 타일은 기본적으로 드래그가 불가능한 "표시 전용" 타일로, WIN/LOSE 글자 패턴의 일부(bIsPatternLit)를
 * 켜고 끄는 용도로만 쓰인다. 오직 우측 하단의 1x2 슬라이더 타일 두 개만 bIsDraggable = true로 설정되어
 * 아이폰 잠금 해제처럼 드래그할 수 있다.
 */
UCLASS()
class SCRATCHANIMAL_API ASAResultTileActor : public AActor
{
	GENERATED_BODY()

public:
	ASAResultTileActor();

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------
	// 컴포넌트
	// -------------------------------------------------------------

	/** 충돌 콜라이더 (루트 컴포넌트) - 포인터/터치 히트 감지용 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoxCollider;

	/** 타일 외형 메쉬 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TileMesh;

	// -------------------------------------------------------------
	// 타일 상태
	// -------------------------------------------------------------

	/** 타일의 그리드 좌표 (X: 열, Y: 행) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result Tile")
	FIntPoint GridCoord;

	/** WIN/LOSE 글자 패턴의 일부로 켜져 있는(밝게 표시되는) 타일인지 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result Tile")
	bool bIsPatternLit;

	/** 드래그 가능한 타일인지 여부 (기본 false - 우측 하단 1x2 슬라이더 타일 두 개만 true) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result Tile")
	bool bIsDraggable;

	/** 현재 드래그 경로에 포함되어 선택된 상태인지 여부 (드래그 가능한 타일에서만 의미 있음) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result Tile")
	bool bIsSelected;

	// -------------------------------------------------------------
	// 색상 설정
	// -------------------------------------------------------------

	/** 패턴이 꺼져 있는(배경) 타일 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Tile|Color")
	FLinearColor BackgroundColor;

	/** WIN/LOSE 글자 패턴이 켜진 타일 색상 (USAResultManager가 결과에 따라 초록/빨강으로 지정) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Tile|Color")
	FLinearColor PatternLitColor;

	/** 드래그 가능한 슬라이더 타일의 기본 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Tile|Color")
	FLinearColor DraggableColor;

	/** 드래그로 선택된 상태일 때 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result Tile|Color")
	FLinearColor SelectedColor;

	// -------------------------------------------------------------
	// 주요 함수 인터페이스
	// -------------------------------------------------------------

	/** 그리드 좌표 설정 */
	UFUNCTION(BlueprintCallable, Category = "Result Tile")
	void SetGridCoord(int32 InX, int32 InY);

	/** 글자 패턴 On/Off 설정 및 시각적 표현 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Result Tile")
	void SetPatternLit(bool bInLit);

	/** 드래그 가능 여부 설정 및 시각적 표현 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Result Tile")
	void SetDraggable(bool bInDraggable);

	/** 드래그 선택 상태 설정 */
	UFUNCTION(BlueprintCallable, Category = "Result Tile")
	void SetSelected(bool bInSelected);

	/** 현재 상태(패턴/드래그 가능/선택 여부)에 맞춰 외형(머티리얼 색상) 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Result Tile")
	void UpdateVisual();

	/** 그리드 좌표 반환 */
	UFUNCTION(BlueprintPure, Category = "Result Tile")
	FORCEINLINE FIntPoint GetGridCoord() const { return GridCoord; }

	/** 글자 패턴 On 여부 반환 */
	UFUNCTION(BlueprintPure, Category = "Result Tile")
	FORCEINLINE bool IsPatternLit() const { return bIsPatternLit; }

	/** 드래그 가능 여부 반환 */
	UFUNCTION(BlueprintPure, Category = "Result Tile")
	FORCEINLINE bool IsDraggable() const { return bIsDraggable; }

	/** 드래그 선택 여부 반환 */
	UFUNCTION(BlueprintPure, Category = "Result Tile")
	FORCEINLINE bool IsSelected() const { return bIsSelected; }

private:
	/** 런타임 색상 변경을 위한 동적 머티리얼 인스턴스 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	/** 동적 머티리얼 생성 및 준비 */
	void EnsureDynamicMaterial();

	/** 현재 상태에 해당하는 기본 색상 반환 */
	FLinearColor GetColorForCurrentState() const;
};
