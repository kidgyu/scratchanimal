// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SATypes.h"
#include "SATileActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UCLASS()
class SCRATCHANIMAL_API ASATileActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ASATileActor();

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

	/** 충돌 콜라이더 (루트 컴포넌트) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoxCollider;

	/** 타일 외형 메쉬 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TileMesh;

	/** 웨이포인트 순서 번호 표시용 3D 텍스트 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> WaypointText;

	// -------------------------------------------------------------
	// 타일 속성
	// -------------------------------------------------------------

	/** 타일 종류 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	ESATileType TileType;

	/** 웨이포인트 순서 번호 (1, 2, 3, 4...) - 웨이포인트 타일일 경우 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile", meta = (ClampMin = "1", EditCondition = "TileType == ESATileType::Waypoint"))
	int32 WaypointIndex;

	/**
	 * 이 웨이포인트에 도착했을 때 카메라를 이동시킬지 여부 (레벨 16~20의 카메라 이동 기믹용).
	 * 웨이포인트 타일일 경우에만 의미가 있다. 실제로 카메라가 바라볼 지점은 CameraFocusCoord.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile", meta = (EditCondition = "TileType == ESATileType::Waypoint"))
	bool bMoveCameraOnArrival;

	/**
	 * bMoveCameraOnArrival이 true일 때, 카메라가 이동해서 바라볼 그리드 좌표. 이 웨이포인트 자신의
	 * 좌표일 필요가 없다 - 다음 웨이포인트나 목표 타일 좌표를 지정해서, 도착 즉시 "다음에 갈 곳"이
	 * 화면에 보이도록 미리 카메라를 옮겨둘 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile", meta = (EditCondition = "TileType == ESATileType::Waypoint && bMoveCameraOnArrival"))
	FIntPoint CameraFocusCoord;

	/** 타일의 그리드 좌표 (X: 열, Y: 행) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
	FIntPoint GridCoord;

	/** 현재 드래그 선택 상태 (선택 시 빨간색으로 변경됨) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile|State")
	bool bIsSelected;

	/** 함정(Trap) 타일 여부 - Normal/Trap을 오가며 깜빡이는 타일인지 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile|State")
	bool bIsTrapTile;

	/**
	 * 현재 몬스터 액터가 이 타일 위에 있는지 여부. 타일의 실제 타입(TileType)은 전혀 바꾸지 않고,
	 * 몬스터가 서 있는 동안에만 바닥 색상을 임시로 진한 보라색(MonsterOverlayColor)으로 표시하기 위한 상태다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile|State")
	bool bIsMonsterOccupied;

	// -------------------------------------------------------------
	// 함정 타일 깜빡임(Trap Blink) 설정
	// -------------------------------------------------------------

	/** 함정 타일의 초기(시작) 상태 - Normal로 시작할지 Trap으로 시작할지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Trap")
	ESATileTrapState InitialTrapState;

	/**
	 * 함정 타일이 활성화되기 전 최초 대기 시간(초). 0이면 대기 없이 바로 InitialTrapState부터 Normal/Trap 사이클이 시작되고,
	 * 0보다 크면 이 시간 동안은 항상 Normal(안전) 상태로 표시되다가, 대기 시간이 끝나는 순간 InitialTrapState로 전환되며
	 * 그때부터 NormalDuration/TrapDuration 설정에 따른 전환 사이클이 시작된다.
	 * 같은 주기의 다른 함정 타일과 이 값만 다르게 주면, 그 차이만큼 위상이 어긋난 "교차" 패턴을 만들 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Trap", meta = (ClampMin = "0.0"))
	float InitialDelay;

	/** Normal 상태 유지 시간(초) - 이 시간이 지나면 Trap 상태로 전환됨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Trap", meta = (ClampMin = "0.1"))
	float NormalDuration;

	/** Trap 상태 유지 시간(초) - 이 시간이 지나면 Normal 상태로 전환됨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Trap", meta = (ClampMin = "0.1"))
	float TrapDuration;

	/** 초기 상태 및 Normal/Trap 유지 시간을 설정 (SetTileType으로 Trap이 지정되어 타이머가 최초 생성되기 전에 호출해야 반영됨) */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetTrapCycleConfig(ESATileTrapState InInitialState, float InNormalDuration, float InTrapDuration);

	/** 최초 대기 시간을 설정 (SetTileType으로 Trap이 지정되어 타이머가 최초 생성되기 전에 호출해야 반영됨) */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetTrapInitialDelay(float InInitialDelay);

	// -------------------------------------------------------------
	// 타일 색상 설정
	// -------------------------------------------------------------

	/** 드래그 선택 시 표시될 색상 (기본: 빨간색) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Color")
	FLinearColor SelectedColor;

	/** 일반 타일 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Color")
	FLinearColor NormalColor;

	/** 시작 타일 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Color")
	FLinearColor StartColor;

	/** 벽 타일 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Color")
	FLinearColor WallColor;

	/** 웨이포인트 타일 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Color")
	FLinearColor WaypointColor;

	/** 함정 타일 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Color")
	FLinearColor TrapColor;

	/** 목표 타일 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Color")
	FLinearColor GoalColor;

	/** 몬스터 액터가 이 타일 위에 있을 때 임시로 표시되는 색상 (기본: 진한 보라색) - 다른 어떤 상태보다 우선해서 표시됨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Color")
	FLinearColor MonsterOverlayColor;

	// -------------------------------------------------------------
	// 웨이포인트 텍스트 설정
	// -------------------------------------------------------------

	/** 웨이포인트 텍스트 상대 회전값 (탑다운 뷰 정면) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|WaypointText")
	FRotator WaypointTextRotation;

	/** 웨이포인트 텍스트 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|WaypointText")
	FColor WaypointTextColor;

	/** 웨이포인트 텍스트 월드 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|WaypointText")
	float WaypointTextWorldSize;

	// -------------------------------------------------------------
	// 주요 함수 인터페이스
	// -------------------------------------------------------------

	/** 타일 종류 설정 및 시각적 표현 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetTileType(ESATileType NewType);

	/** 그리드 좌표 설정 */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetGridCoord(int32 InX, int32 InY);

	/** 웨이포인트 인덱스 설정 */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetWaypointIndex(int32 InIndex);

	/** 이 웨이포인트 도착 시 카메라 이동 여부 설정 */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetMoveCameraOnArrival(bool bInMoveCamera);

	/** 카메라 이동 시 바라볼 그리드 좌표 설정 (이 웨이포인트 자신의 좌표가 아니어도 됨) */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetCameraFocusCoord(const FIntPoint& InCoord);

	/** 드래그 선택 상태 설정 (선택 시 빨간색 등으로 하이라이트) */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetSelected(bool bInSelected);

	/**
	 * 몬스터 액터가 이 타일 위에 있는지 여부를 설정한다 (ASAMonsterActor가 이동하며 호출).
	 * 타일 타입은 그대로 유지한 채, 몬스터가 있는 동안만 바닥이 진한 보라색으로 표시된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetMonsterOccupied(bool bInOccupied);

	/** 현재 상태(타일 타입 및 선택 여부)에 맞춰 외형(머티리얼 색상) 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void UpdateVisual();

	/** 현재 타일 타입에 해당하는 기본 색상 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile")
	FLinearColor GetColorForTileType(ESATileType InType) const;

	/** 충돌 콜라이더 반환 */
	FORCEINLINE UBoxComponent* GetBoxCollider() const { return BoxCollider; }

	/** 메쉬 컴포넌트 반환 */
	FORCEINLINE UStaticMeshComponent* GetTileMesh() const { return TileMesh; }

	/** 웨이포인트 텍스트 컴포넌트 반환 */
	FORCEINLINE UTextRenderComponent* GetWaypointText() const { return WaypointText; }

	/** 그리드 좌표 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile")
	FORCEINLINE FIntPoint GetGridCoord() const { return GridCoord; }

	/** 타일 종류 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile")
	FORCEINLINE ESATileType GetTileType() const { return TileType; }

	/** 웨이포인트 번호 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile")
	FORCEINLINE int32 GetWaypointIndex() const { return WaypointIndex; }

	/** 이 웨이포인트 도착 시 카메라 이동 여부 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile")
	FORCEINLINE bool ShouldMoveCameraOnArrival() const { return bMoveCameraOnArrival; }

	/** 카메라 이동 시 바라볼 그리드 좌표 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile")
	FORCEINLINE FIntPoint GetCameraFocusCoord() const { return CameraFocusCoord; }

	/** 드래그 선택 여부 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile")
	FORCEINLINE bool IsSelected() const { return bIsSelected; }

	/** 몬스터 액터가 이 타일 위에 있는지 여부 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile")
	FORCEINLINE bool IsMonsterOccupied() const { return bIsMonsterOccupied; }

private:
	/** 런타임 색상 변경을 위한 동적 머티리얼 인스턴스 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	/** 함정 타일 상태 전환 타이머 핸들 */
	FTimerHandle TrapStateTimerHandle;

	/** 현재 Normal/Trap 중 어느 상태인지 (bIsTrapTile인 동안 계속 갱신됨) */
	ESATileTrapState CurrentTrapState;

	/** 동적 머티리얼 생성 및 준비 */
	void EnsureDynamicMaterial();

	/**
	 * 함정 상태 전환 사이클을 시작한다. CurrentTrapState를 InitialTrapState로 설정하고,
	 * InitialDelay가 0보다 크면 그 시간이 지난 뒤 BeginTrapCycleAfterDelay가, 0이면 곧바로
	 * 해당 상태의 유지 시간이 지난 뒤 ToggleTrapState가 호출되도록 예약한다.
	 */
	void StartTrapBlinkTimer();

	/** 함정 상태 전환 타이머를 완전히 제거 (더 이상 함정 타일이 아니게 될 때만 호출) */
	void StopTrapBlinkTimer();

	/** InitialDelay가 지난 뒤 한 번 호출되어, 그제서야 CurrentTrapState의 유지 시간에 따른 정식 전환 사이클을 예약하기 시작한다 */
	void BeginTrapCycleAfterDelay();

	/**
	 * CurrentTrapState를 Normal ↔ Trap으로 뒤바꾸고, 새 상태의 유지 시간(NormalDuration/TrapDuration)만큼
	 * 지난 뒤 다시 이 함수가 호출되도록 타이머를 예약한다. 선택된 동안에는 화면 갱신만 건너뛰고 상태 전환과
	 * 타이머 예약 자체는 계속 정확히 진행된다.
	 */
	void ToggleTrapState();

	/** CurrentTrapState를 TileType에 반영해 시각적으로 갱신한다 (선택 중이면 건너뜀) */
	void ApplyTrapVisualState();
};
