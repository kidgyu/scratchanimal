// Fill out your copyright notice in the Description page of Project Settings.

#include "SAGameMode.h"
#include "SAGameState.h"
#include "SAPlayerPawn.h"
#include "SAPlayerController.h"

ASAGameMode::ASAGameMode()
{
	DefaultPawnClass = ASAPlayerPawn::StaticClass();
	GameStateClass = ASAGameState::StaticClass();
	PlayerControllerClass = ASAPlayerController::StaticClass();
}
