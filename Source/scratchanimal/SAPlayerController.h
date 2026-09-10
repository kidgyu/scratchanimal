// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SAPlayerController.generated.h"

class ASATileActor;
class ASAMenuTileActor;
class ASAResultTileActor;

/**
 * 화면 터치 및 마우스 드래그를 감지하여
 * SATileManager(인게임), SAMenuManager(메뉴 화면), SAResultManager(결과 화면) 중 알맞은 곳에
 * 타일 선택 및 경로 검증 이벤트를 전달하는 플레이어 컨트롤러.
 * 세 타일 그리드는 항상 그중 하나만 월드에 존재하므로, 히트된 액터의 타입만 보고 알맞은 매니저로 라우팅한다.
 */
UCLASS()
class SCRATCHANIMAL_API ASAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASAPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

public:
	/** 현재 마우스 커서 또는 터치 손가락 아래에 위치한 액터 반환 (타입 무관) */
	UFUNCTION(BlueprintPure, Category = "Player Controller|Input")
	AActor* GetActorUnderPointer() const;

	/** 현재 마우스 커서 또는 터치 손가락 아래에 위치한 인게임 타일 액터 반환 */
	UFUNCTION(BlueprintPure, Category = "Player Controller|Input")
	ASATileActor* GetTileUnderPointer() const;

	/** 현재 마우스 커서 또는 터치 손가락 아래에 위치한 메뉴 타일 액터 반환 */
	UFUNCTION(BlueprintPure, Category = "Player Controller|Input")
	ASAMenuTileActor* GetMenuTileUnderPointer() const;

	/** 현재 마우스 커서 또는 터치 손가락 아래에 위치한 결과 화면 타일 액터 반환 */
	UFUNCTION(BlueprintPure, Category = "Player Controller|Input")
	ASAResultTileActor* GetResultTileUnderPointer() const;

	/** 현재 포인터(마우스/터치)가 눌려있는 상태인지 여부 */
	UFUNCTION(BlueprintPure, Category = "Player Controller|Input")
	bool IsPointerDown() const { return bIsPointerDown; }

protected:
	/** 마우스 좌클릭 누름 핸들러 */
	void OnMousePressed();

	/** 마우스 좌클릭 해제 핸들러 */
	void OnMouseReleased();

	/** 화면 터치 시작 핸들러 */
	void OnTouchPressed(ETouchIndex::Type FingerIndex, FVector Location);

	/** 화면 터치 종료 핸들러 */
	void OnTouchReleased(ETouchIndex::Type FingerIndex, FVector Location);

	/** 공통 포인터 누름 처리 */
	void HandlePointerDown();

	/** 공통 포인터 뗌 처리 */
	void HandlePointerUp();

protected:
	/** 포인터가 눌려있는 중인지 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Controller|Input")
	bool bIsPointerDown;

	/** 모바일 터치 중인지 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Controller|Input")
	bool bIsTouching;

	/** 직전 프레임에 감지된 액터 캐시 (인게임 타일 또는 메뉴 타일) */
	UPROPERTY(Transient)
	TObjectPtr<AActor> LastTouchedActor;
};
