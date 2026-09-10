// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SAMenuTileActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/**
 * 레벨 선택 메뉴에서 SATileActor의 드래그 타일 시스템을 그대로 재현하기 위한 메뉴 전용 타일 액터.
 * (0,0)은 항상 시작 타일이고, 그 외 칸은 등록된 레벨이 있으면 번호를 표시하는 "레벨 선택 타일"이 되거나
 * 등록된 레벨이 없으면 그냥 빈 타일(Normal)로 남는다.
 */
UCLASS()
class SCRATCHANIMAL_API ASAMenuTileActor : public AActor
{
	GENERATED_BODY()

public:
	ASAMenuTileActor();

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

	/** 레벨 번호 표시용 3D 텍스트 컴포넌트 (등록된 레벨이 있을 때만 보임) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> LevelNumberText;

	// -------------------------------------------------------------
	// 타일 상태
	// -------------------------------------------------------------

	/** 타일의 그리드 좌표 (X: 열, Y: 행) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu Tile")
	FIntPoint GridCoord;

	/** 이 타일이 드래그 시작 지점(좌측 상단, 그리드 좌표 0,0)인지 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu Tile")
	bool bIsStartTile;

	/** 이 타일에 배정된 레벨 번호 (0이면 등록된 레벨이 없는 빈 타일) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu Tile")
	int32 AssignedLevelNumber;

	/** 현재 드래그 경로에 포함되어 선택된 상태인지 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu Tile")
	bool bIsSelected;

	// -------------------------------------------------------------
	// 색상 설정
	// -------------------------------------------------------------

	/** 빈 타일(등록된 레벨 없음) 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Tile|Color")
	FLinearColor NormalColor;

	/** 시작 타일 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Tile|Color")
	FLinearColor StartColor;

	/** 레벨이 배정된 타일 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Tile|Color")
	FLinearColor LevelColor;

	/** 드래그로 선택된 상태일 때 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Tile|Color")
	FLinearColor SelectedColor;

	// -------------------------------------------------------------
	// 레벨 번호 텍스트 설정
	// -------------------------------------------------------------

	/** 레벨 번호 텍스트 상대 회전값 (탑다운 뷰 정면) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Tile|Text")
	FRotator LevelTextRotation;

	/** 레벨 번호 텍스트 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Tile|Text")
	FColor LevelTextColor;

	/** 레벨 번호 텍스트 월드 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Tile|Text")
	float LevelTextWorldSize;

	// -------------------------------------------------------------
	// 주요 함수 인터페이스
	// -------------------------------------------------------------

	/** 그리드 좌표 설정 */
	UFUNCTION(BlueprintCallable, Category = "Menu Tile")
	void SetGridCoord(int32 InX, int32 InY);

	/** 시작 타일 여부 설정 및 시각적 표현 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Menu Tile")
	void SetIsStartTile(bool bInIsStartTile);

	/** 배정된 레벨 번호 설정 (0이면 빈 타일) 및 시각적 표현 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Menu Tile")
	void SetAssignedLevelNumber(int32 InLevelNumber);

	/** 드래그 선택 상태 설정 */
	UFUNCTION(BlueprintCallable, Category = "Menu Tile")
	void SetSelected(bool bInSelected);

	/** 현재 상태(시작/레벨/선택 여부)에 맞춰 외형(머티리얼 색상, 텍스트) 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Menu Tile")
	void UpdateVisual();

	/** 그리드 좌표 반환 */
	UFUNCTION(BlueprintPure, Category = "Menu Tile")
	FORCEINLINE FIntPoint GetGridCoord() const { return GridCoord; }

	/** 시작 타일 여부 반환 */
	UFUNCTION(BlueprintPure, Category = "Menu Tile")
	FORCEINLINE bool IsStartTile() const { return bIsStartTile; }

	/** 배정된 레벨 번호 반환 (0이면 빈 타일) */
	UFUNCTION(BlueprintPure, Category = "Menu Tile")
	FORCEINLINE int32 GetAssignedLevelNumber() const { return AssignedLevelNumber; }

	/** 등록된 레벨을 가진 타일인지 여부 */
	UFUNCTION(BlueprintPure, Category = "Menu Tile")
	FORCEINLINE bool HasAssignedLevel() const { return AssignedLevelNumber > 0; }

	/** 드래그 선택 여부 반환 */
	UFUNCTION(BlueprintPure, Category = "Menu Tile")
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
