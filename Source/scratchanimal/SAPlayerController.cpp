// Fill out your copyright notice in the Description page of Project Settings.

#include "SAPlayerController.h"
#include "SATileActor.h"
#include "SATileManager.h"
#include "SAMenuTileActor.h"
#include "SAMenuManager.h"
#include "SAResultTileActor.h"
#include "SAResultManager.h"
#include "Engine/World.h"

ASAPlayerController::ASAPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	bEnableTouchEvents = true;
	bEnableTouchOverEvents = true;

	bIsPointerDown = false;
	bIsTouching = false;
	LastTouchedActor = nullptr;
}

void ASAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 입력 모드를 게임 및 UI로 설정하여 마우스와 터치가 즉시 동작하도록 설정
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ASAPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		// 마우스 좌클릭 바인딩
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASAPlayerController::OnMousePressed);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ASAPlayerController::OnMouseReleased);

		// 모바일 화면 터치 바인딩
		InputComponent->BindTouch(IE_Pressed, this, &ASAPlayerController::OnTouchPressed);
		InputComponent->BindTouch(IE_Released, this, &ASAPlayerController::OnTouchReleased);
	}
}

void ASAPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// 마우스 또는 터치로 누른 채 화면을 드래그하고 있을 때 실시간 타일 감지
	if (bIsPointerDown)
	{
		AActor* CurrentActor = GetActorUnderPointer();
		if (CurrentActor && CurrentActor != LastTouchedActor)
		{
			LastTouchedActor = CurrentActor;

			if (ASAMenuTileActor* MenuTile = Cast<ASAMenuTileActor>(CurrentActor))
			{
				if (USAMenuManager* MenuManager = USAMenuManager::Get(this))
				{
					MenuManager->ProcessTileTouchMove(MenuTile);
				}
			}
			else if (ASAResultTileActor* ResultTile = Cast<ASAResultTileActor>(CurrentActor))
			{
				if (USAResultManager* ResultManager = USAResultManager::Get(this))
				{
					ResultManager->ProcessTileTouchMove(ResultTile);
				}
			}
			else if (ASATileActor* GameTile = Cast<ASATileActor>(CurrentActor))
			{
				if (USATileManager* TileManager = USATileManager::Get(this))
				{
					TileManager->ProcessTileTouchMove(GameTile);
				}
			}
		}
	}
}

AActor* ASAPlayerController::GetActorUnderPointer() const
{
	FHitResult HitResult;
	bool bHit = false;

	if (bIsTouching)
	{
		bHit = GetHitResultUnderFinger(ETouchIndex::Touch1, ECC_Visibility, true, HitResult);
	}
	else
	{
		bHit = GetHitResultUnderCursor(ECC_Visibility, true, HitResult);
	}

	return bHit ? HitResult.GetActor() : nullptr;
}

ASATileActor* ASAPlayerController::GetTileUnderPointer() const
{
	return Cast<ASATileActor>(GetActorUnderPointer());
}

ASAMenuTileActor* ASAPlayerController::GetMenuTileUnderPointer() const
{
	return Cast<ASAMenuTileActor>(GetActorUnderPointer());
}

ASAResultTileActor* ASAPlayerController::GetResultTileUnderPointer() const
{
	return Cast<ASAResultTileActor>(GetActorUnderPointer());
}

void ASAPlayerController::OnMousePressed()
{
	bIsTouching = false;
	HandlePointerDown();
}

void ASAPlayerController::OnMouseReleased()
{
	bIsTouching = false;
	HandlePointerUp();
}

void ASAPlayerController::OnTouchPressed(ETouchIndex::Type FingerIndex, FVector Location)
{
	if (FingerIndex == ETouchIndex::Touch1)
	{
		bIsTouching = true;
		HandlePointerDown();
	}
}

void ASAPlayerController::OnTouchReleased(ETouchIndex::Type FingerIndex, FVector Location)
{
	if (FingerIndex == ETouchIndex::Touch1)
	{
		bIsTouching = false;
		HandlePointerUp();
	}
}

void ASAPlayerController::HandlePointerDown()
{
	bIsPointerDown = true;

	AActor* TouchedActor = GetActorUnderPointer();
	LastTouchedActor = TouchedActor;

	if (ASAMenuTileActor* MenuTile = Cast<ASAMenuTileActor>(TouchedActor))
	{
		if (USAMenuManager* MenuManager = USAMenuManager::Get(this))
		{
			MenuManager->ProcessTileTouchBegin(MenuTile);
		}
		return;
	}

	if (ASAResultTileActor* ResultTile = Cast<ASAResultTileActor>(TouchedActor))
	{
		if (USAResultManager* ResultManager = USAResultManager::Get(this))
		{
			ResultManager->ProcessTileTouchBegin(ResultTile);
		}
		return;
	}

	if (ASATileActor* GameTile = Cast<ASATileActor>(TouchedActor))
	{
		if (USATileManager* TileManager = USATileManager::Get(this))
		{
			TileManager->ProcessTileTouchBegin(GameTile);
		}
	}
}

void ASAPlayerController::HandlePointerUp()
{
	bIsPointerDown = false;
	LastTouchedActor = nullptr;

	// 세 매니저 모두 자기 경로가 비어있으면 아무 것도 하지 않으므로, 현재 어느 화면인지 몰라도 안전하게 다 호출 가능
	if (USATileManager* TileManager = USATileManager::Get(this))
	{
		TileManager->ProcessTileTouchEnd();
	}

	if (USAMenuManager* MenuManager = USAMenuManager::Get(this))
	{
		MenuManager->ProcessTileTouchEnd();
	}

	if (USAResultManager* ResultManager = USAResultManager::Get(this))
	{
		ResultManager->ProcessTileTouchEnd();
	}
}
