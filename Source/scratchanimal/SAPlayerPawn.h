// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SATypes.h"
#include "SAPlayerPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USceneComponent;

/**
 * 타일 퍼즐 게임을 위한 탑다운 시점 폰
 * 레벨 시작 시 타일 그리드의 중심점과 크기를 계산하여 카메라를 자동으로 포커싱합니다.
 */
UCLASS()
class SCRATCHANIMAL_API ASAPlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	ASAPlayerPawn();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 타일 그리드의 중심과 크기에 맞춰 폰 위치 및 카메라 거리를 자동 조정 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void FocusOnTileGrid();

	/** Result 화면 표시 시 호출 - ResultScreenReferenceLevel(기본 10) 레벨을 열었을 때와 동일한 카메라 위치/거리로 맞춘다 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void FocusOnResultScreen();

	/**
	 * 메뉴 화면 표시 시 호출 - SAMenuManager의 그리드 중심/크기에 맞춰 카메라를 되돌린다. 인게임 중
	 * 카메라 이동 웨이포인트(레벨 16~20)로 폰이 그리드 중심에서 멀리 이동해 있었더라도, 메뉴로 돌아오면
	 * 항상 처음 메뉴를 열었을 때와 동일한 위치/거리로 복귀시키기 위함이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void FocusOnMenu();

	/**
	 * 실제 화면 좌표(뷰포트 우상단 모서리에서 MenuButtonScreenMarginRatio만큼 안쪽)를 카메라 투영으로
	 * 역산해, 폰의 현재 위치 + InHeightOffset 높이의 평면과 만나는 정확한 월드 위치를 구한 뒤
	 * 폰 기준 로컬 오프셋으로 OutOffset에 담는다. FOV/종횡비/카메라 기울기와 무관하게 항상 실제 화면
	 * 우상단에 정확히 대응하므로, 그 지점에 항상 붙여두고 싶은 인게임 요소(우상단 나가기 슬라이더 등)를
	 * 폰에 부착할 때 이 오프셋을 사용한다.
	 * @param InHeightOffset 그리드 표면(폰의 현재 Z) 기준으로 얼마나 위에 배치할지 (겹침 방지용)
	 * @param OutOffset 계산에 성공했을 때만 유효한 폰 기준 로컬 오프셋
	 * @return 뷰포트 크기를 아직 알 수 없는 등의 이유로 계산에 실패하면 false (호출 측에서 잠시 뒤 재시도해야 함 -
	 *         실패 시에도 OutOffset을 그대로 써버리면 (0,0,InHeightOffset), 즉 폰 위치에 겹쳐서 스폰되는 문제가 생긴다)
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	bool GetTopRightCornerOffset(float InHeightOffset, FVector& OutOffset) const;

	/** 화면 우상단 모서리에서 얼마나 안쪽으로 들여서 GetTopRightCornerOffset을 계산할지 (뷰포트 비율, 작을수록 모서리에 더 붙음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector2D MenuButtonScreenMarginRatio;

	// -------------------------------------------------------------
	// 카메라 옵션
	// -------------------------------------------------------------

	/** 카메라 각도 (기본: -89.9도 완전 탑다운. -65도 등으로 변경 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FRotator CameraRotation;

	/** 화면 테두리 여백 비율 (1.0 = 완전히 꽉 채움, 1.05 = 화면 꽉 차게 5% 여유) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraPaddingRatio;

	/** 화면 안전 경계 여백 비율 (0.04 = 상하좌우 4% 여유를 두고 화면에 꽉 차게 배치) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ScreenMarginRatio;

	/** 최소 카메라 거리 (300 = 작은 그리드도 화면에 꽉 차도록 지원) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinCameraDistance;

	/** 레벨이 로드될 때 자동으로 그리드에 카메라를 맞출지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bAutoFocusOnLevelLoaded;

	/** FocusOnResultScreen에서 기준으로 삼을 레벨 번호 (기본 10 - 마일스톤 레벨을 열었을 때와 동일한 카메라가 되도록) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	int32 ResultScreenReferenceLevel;

	/**
	 * 카메라가 줄 수 있는 최대 거리의 기준이 되는 레벨 번호 (기본 15 - 10x10 그리드까지 잘리지 않게). 그리드가 이 레벨보다 커도 카메라는
	 * 이 레벨의 그리드 크기가 요구하는 거리보다 더 멀어지지 않는다. 카메라 이동 기믹이 도입되는 레벨
	 * 16~20처럼 그리드가 더 커지는 경우, 줌아웃 대신 카메라가 웨이포인트를 따라 패닝하도록 하기 위함.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	int32 MaxCameraDistanceReferenceLevel;

	/** 카메라 이동 웨이포인트 도착 시, 그 타일 위치까지 Lerp로 카메라를 이동시키는 데 걸리는 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.01"))
	float WaypointCameraMoveDuration;

	/** 화면 가로/세로 영역에서 타일이 잘리거나 나갔는지 체크하여 폰 위치와 카메라 거리를 정밀 보정 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void CheckAndAdjustCameraBounds();

	/** Returns SpringArmComponent subobject **/
	FORCEINLINE USpringArmComponent* GetSpringArmComponent() const { return SpringArmComp; }

	/** Returns CameraComponent subobject **/
	FORCEINLINE UCameraComponent* GetCameraComponent() const { return CameraComp; }

protected:
	/** 레벨 로드 시 호출될 핸들러 */
	UFUNCTION()
	void OnLevelLoadedHandler(int32 LevelNumber);

	/** GameState가 Result 상태로 바뀔 때 호출될 핸들러 */
	UFUNCTION()
	void OnGameStateChangedHandler(ESAGameStateType NewState, ESAGameStateType PrevState);

	/** SATileManager의 카메라 이동 웨이포인트 도착 이벤트 핸들러 - 목표 위치로의 Lerp 이동을 시작한다 */
	UFUNCTION()
	void OnWaypointCameraMoveRequestedHandler(FVector TargetLocation);

private:
	/** 지정된 중심 위치/그리드 크기/타일 간격에 맞춰 폰 위치 및 카메라 거리를 계산 (FocusOnTileGrid/FocusOnResultScreen 공용) */
	void FocusCameraOnGrid(const FVector& InCenter, int32 InGridWidth, int32 InGridHeight, float InTileStep);

	/** Root scene component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Camera boom positioning the camera behind the pawn */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArmComp;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> CameraComp;

	/** 화면 경계 체크용 타이머 핸들 */
	FTimerHandle BoundsTimerHandle;

	// -------------------------------------------------------------
	// 카메라 이동 웨이포인트 Lerp 이동 상태
	// -------------------------------------------------------------

	/** 현재 카메라 이동 웨이포인트로 Lerp 이동 중인지 여부 */
	bool bIsPanningToWaypoint;

	/** 이동 구간의 시작 위치 */
	FVector WaypointPanStartLocation;

	/** 이동 목표 위치 (카메라 이동 웨이포인트 타일 위치) */
	FVector WaypointPanTargetLocation;

	/** 이동 구간이 시작된 뒤 흐른 시간(초) */
	float WaypointPanElapsedTime;
};
