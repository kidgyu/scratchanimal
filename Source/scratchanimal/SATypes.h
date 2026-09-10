// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SATypes.generated.h"

/**
 * 타일의 종류를 정의하는 열거형
 */
UENUM(BlueprintType)
enum class ESATileType : uint8
{
	Normal UMETA(DisplayName = "Normal"),			// 일반 타일 (기본 바닥)
	Start UMETA(DisplayName = "Start"),				// 시작 타일
	Wall UMETA(DisplayName = "Wall"),				// 벽 타일 (이동 불가)
	Waypoint UMETA(DisplayName = "Waypoint"),		// 웨이포인트 타일 (순서대로 경유 필요)
	Trap UMETA(DisplayName = "Trap"),				// 함정 타일
	Goal UMETA(DisplayName = "Goal")				// 목표 타일 (최종 도착점)
};

/**
 * 함정(Trap) 타일이 깜빡이기 시작할 때의 초기 상태
 */
UENUM(BlueprintType)
enum class ESATileTrapState : uint8
{
	Normal UMETA(DisplayName = "Normal"),	// Normal 상태로 시작
	Trap UMETA(DisplayName = "Trap")		// Trap 상태로 시작
};

/**
 * 게임 결과(승/패)를 정의하는 열거형
 */
UENUM(BlueprintType)
enum class ESAResultType : uint8
{
	Win UMETA(DisplayName = "Win"),		// 승리 (마일스톤 레벨 클리어)
	Lose UMETA(DisplayName = "Lose")	// 패배 (함정에 걸림)
};

/**
 * 게임의 진행 상태를 정의하는 열거형
 */
UENUM(BlueprintType)
enum class ESAGameStateType : uint8
{
	Menu UMETA(DisplayName = "Menu"),				// 메인 메뉴 상태
	Game UMETA(DisplayName = "Game"),				// 인게임 플레이 상태 (타일 생성 및 퍼즐 진행)
	Result UMETA(DisplayName = "Result")			// 게임 결과 (성공/실패) 상태
};

UENUM(BlueprintType)
enum class ESABGMType : uint8
{
	None,
	Menu,
	Ingame,
	Result
};

UENUM(BlueprintType)
enum class ESASFXType : uint8
{
	None,
	Button,

	Normal,
	WayPoint,
	Trap,
	Goal,
	NextLevel,

	GameOver_Voice
};