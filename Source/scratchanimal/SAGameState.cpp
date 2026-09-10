// Fill out your copyright notice in the Description page of Project Settings.

#include "SAGameState.h"
#include "SATileManager.h"
#include "SATileSettings.h"
#include "SASoundManager.h"
#include "SAActorManager.h"
#include "SAFoodActor.h"
#include "SAMenuManager.h"
#include "SAResultManager.h"
#include "TimerManager.h"

ASAGameState::ASAGameState()
{
	InitialStateType = ESAGameStateType::Menu;
	InitialLevel = 1;
	CurrentStateType = ESAGameStateType::Menu;
	CurrentLevel = 1;
	bHasInitializedGame = false;
	NextLevelTransitionDelay = 1.0f;
	MilestoneLevelInterval = 10;
	PendingResultType = ESAResultType::Lose;
}

void ASAGameState::BeginPlay()
{
	Super::BeginPlay();

	// SATileManager의 레벨 클리어 이벤트에 바인딩
	if (USATileManager* TileManager = USATileManager::Get(this))
	{
		TileManager->OnLevelCompleted.RemoveDynamic(this, &ASAGameState::OnTileLevelCompleted);
		TileManager->OnLevelCompleted.AddDynamic(this, &ASAGameState::OnTileLevelCompleted);
	}

	// SAMenuManager의 레벨 선택 이벤트에 바인딩
	if (USAMenuManager* MenuManager = USAMenuManager::Get(this))
	{
		MenuManager->OnLevelSelected.RemoveDynamic(this, &ASAGameState::OnMenuLevelSelected);
		MenuManager->OnLevelSelected.AddDynamic(this, &ASAGameState::OnMenuLevelSelected);
	}

	// SATileManager의 Trap 발동 / 타일 와이프 완료 이벤트에 바인딩
	if (USATileManager* TileManager = USATileManager::Get(this))
	{
		TileManager->OnTrapTriggered.RemoveDynamic(this, &ASAGameState::HandleTrapTriggered);
		TileManager->OnTrapTriggered.AddDynamic(this, &ASAGameState::HandleTrapTriggered);

		TileManager->OnMonsterCaughtPlayer.RemoveDynamic(this, &ASAGameState::HandleMonsterCaughtPlayer);
		TileManager->OnMonsterCaughtPlayer.AddDynamic(this, &ASAGameState::HandleMonsterCaughtPlayer);

		TileManager->OnMenuButtonSwiped.RemoveDynamic(this, &ASAGameState::HandleMenuButtonSwiped);
		TileManager->OnMenuButtonSwiped.AddDynamic(this, &ASAGameState::HandleMenuButtonSwiped);

		TileManager->OnTileWipeCompleted.RemoveDynamic(this, &ASAGameState::HandleTileWipeCompleted);
		TileManager->OnTileWipeCompleted.AddDynamic(this, &ASAGameState::HandleTileWipeCompleted);
	}

	// SAResultManager의 잠금 해제(슬라이더 드래그) 이벤트에 바인딩
	if (USAResultManager* ResultManager = USAResultManager::Get(this))
	{
		ResultManager->OnResultUnlockSwiped.RemoveDynamic(this, &ASAGameState::HandleResultUnlockSwiped);
		ResultManager->OnResultUnlockSwiped.AddDynamic(this, &ASAGameState::HandleResultUnlockSwiped);
	}

	UWorld* World = GetWorld();
	if (World)
	{
		// 이미 월드의 BeginPlay가 완료된 상태(PIE/Standalone 환경 등)라면 즉시 초기화 실행
		if (World->HasBegunPlay())
		{
			OnAllActorsBeginPlayCompleted();
		}
		else
		{
			// 아직 모든 액터의 BeginPlay가 끝나기 전이라면 델리게이트에 바인딩
			World->OnWorldBeginPlay.AddUObject(this, &ASAGameState::OnAllActorsBeginPlayCompleted);
		}
	}
}

void ASAGameState::OnAllActorsBeginPlayCompleted()
{
	// 중복 초기화 방지
	if (bHasInitializedGame)
	{
		return;
	}
	bHasInitializedGame = true;

	UE_LOG(LogTemp, Display, TEXT("[ASAGameState] OnAllActorsBeginPlayCompleted 실행! 초기 상태: %s, 초기 레벨: %d"),
		*UEnum::GetValueAsString(InitialStateType), InitialLevel);

	// 게임 시작 시 초기 설정된 상태로 진입
	SetGameStateType(InitialStateType, InitialLevel);
}

void ASAGameState::SetGameStateType(ESAGameStateType NewState, int32 TargetLevel)
{
	const ESAGameStateType PrevState = CurrentStateType;
	CurrentStateType = NewState;

	UE_LOG(LogTemp, Log, TEXT("[ASAGameState] 상태 전환: %s -> %s (TargetLevel: %d)"),
		*UEnum::GetValueAsString(PrevState),
		*UEnum::GetValueAsString(NewState),
		TargetLevel);

	switch (NewState)
	{
	case ESAGameStateType::Menu:
		HandleMenuState();
		break;

	case ESAGameStateType::Game:
		CurrentLevel = TargetLevel;
		HandleGameState(CurrentLevel);
		break;

	case ESAGameStateType::Result:
		HandleResultState();
		break;
	}

	OnGameStateChanged.Broadcast(CurrentStateType, PrevState);
}

void ASAGameState::StartGame(int32 LevelNumber)
{
	SetGameStateType(ESAGameStateType::Game, LevelNumber);
}

void ASAGameState::OpenMenu()
{
	SetGameStateType(ESAGameStateType::Menu);
}

void ASAGameState::ShowResult(ESAResultType InResultType)
{
	PendingResultType = InResultType;
	SetGameStateType(ESAGameStateType::Result);
}

void ASAGameState::RestartLevel()
{
	if (CurrentStateType == ESAGameStateType::Game)
	{
		HandleGameState(CurrentLevel);
	}
	else
	{
		StartGame(CurrentLevel);
	}
}

void ASAGameState::NextLevel()
{
	StartGame(CurrentLevel + 1);
}

void ASAGameState::HandleMenuState()
{
	// 메뉴 상태 진입: 인게임 중이던 동물/음식/몬스터 액터와 트랩 이펙트 풀이 남아있다면 완전히 정리
	// (화면 우상단 메뉴 이동 슬라이더처럼 와이프 연출 없이 곧바로 메뉴로 전환되는 경로에서도 반드시 필요)
	if (USAActorManager* ActorManager = USAActorManager::Get(this))
	{
		ActorManager->ClearManagedActors();
		ActorManager->DestroyTrapEffectPool();
	}

	// 메뉴 상태 진입: 기존 게임 타일이 남아있다면 정리
	if (USATileManager* TileManager = USATileManager::Get(this))
	{
		TileManager->ClearTiles();
	}

	// 결과 화면에서 넘어온 경우를 대비해 결과 타일 그리드를 정리
	if (USAResultManager* ResultManager = USAResultManager::Get(this))
	{
		ResultManager->ClearResultTiles();
	}

	// SAMenuManager를 통해 레벨 선택 메뉴 타일 그리드 생성
	if (USAMenuManager* MenuManager = USAMenuManager::Get(this))
	{
		MenuManager->LoadMenu();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ASAGameState] SAMenuManager 서브시스템을 찾을 수 없습니다."));
	}

	// Menu BGM 재생
	if (USASoundManager* SoundManager = USASoundManager::Get(this))
	{
		SoundManager->PlayBGM(ESABGMType::Menu);
	}

	UE_LOG(LogTemp, Log, TEXT("[ASAGameState] Menu 상태 진입"));
}

void ASAGameState::HandleGameState(int32 LevelNumber)
{
	UE_LOG(LogTemp, Log, TEXT("[ASAGameState] Game 상태 진입: SATileManager를 통해 레벨 %d 타일 생성 시작"), LevelNumber);

	// 메뉴 화면에서 넘어온 경우를 대비해 메뉴 타일 그리드를 정리
	if (USAMenuManager* MenuManager = USAMenuManager::Get(this))
	{
		MenuManager->ClearMenuTiles();
	}

	// Ingame BGM 재생 (이미 Ingame BGM이 재생 중이면 중복 재생 없이 연속 재생됨)
	if (USASoundManager* SoundManager = USASoundManager::Get(this))
	{
		SoundManager->PlayBGM(ESABGMType::Ingame);
	}

	// SATileManager를 가져와서 해당 레벨 타일들을 생성
	int32 MonsterCountForLevel = 0;
	if (USATileManager* TileManager = USATileManager::Get(this))
	{
		const bool bSuccess = TileManager->LoadLevel(LevelNumber);
		if (!bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ASAGameState] 레벨 %d 타일 생성 실패!"), LevelNumber);
		}
		else
		{
			MonsterCountForLevel = TileManager->CurrentLevelData.MonsterCount;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ASAGameState] SATileManager 서브시스템을 찾을 수 없습니다."));
	}

	// SAActorManager를 통해 동물/음식 액터 생성 (이미 생성되어 있으면 재사용되고, 새로 만들어지지 않음) 및
	// 이 레벨에 필요한 마리 수에 맞춰 몬스터 액터 스폰/정리
	if (USAActorManager* ActorManager = USAActorManager::Get(this))
	{
		ActorManager->SpawnAnimalActor();
		ActorManager->SpawnFoodActor();
		ActorManager->SpawnMonsterActors(MonsterCountForLevel);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ASAGameState] SAActorManager 서브시스템을 찾을 수 없습니다."));
	}
}

void ASAGameState::HandleResultState()
{
	// Result BGM 재생
	if (USASoundManager* SoundManager = USASoundManager::Get(this))
	{
		SoundManager->PlayBGM(ESABGMType::Result);
	}

	// SAResultManager를 통해 WIN/LOSE 결과 화면(타일 패턴 + 우측 하단 잠금 해제 슬라이더) 생성
	if (USAResultManager* ResultManager = USAResultManager::Get(this))
	{
		ResultManager->ShowResult(PendingResultType);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ASAGameState] SAResultManager 서브시스템을 찾을 수 없습니다."));
	}

	UE_LOG(LogTemp, Log, TEXT("[ASAGameState] Result 상태 진입 (%s)"), *UEnum::GetValueAsString(PendingResultType));
}

void ASAGameState::OnTileLevelCompleted(int32 CompletedLevelNumber)
{
	// 목표 타일에 도착한 순간 음식 액터는 바로 사라지고, 그 자리에 먹는 이펙트가 재생됨 (동물 액터는 그대로 유지)
	if (USAActorManager* ActorManager = USAActorManager::Get(this))
	{
		FVector EffectLocation = FVector::ZeroVector;
		if (ASAFoodActor* FoodActor = ActorManager->GetFoodActor())
		{
			EffectLocation = FoodActor->GetActorLocation();
		}

		ActorManager->ClearFoodActor();
		ActorManager->SpawnFoodEffectActor(EffectLocation);
	}

	// N단계(MilestoneLevelInterval) 단위로 레벨을 깬 경우: 다음 레벨로 바로 넘어가지 않고
	// 레벨 이탈 연출(타일 와이프)을 재생한 뒤 WIN 결과 화면을 보여준다.
	if (MilestoneLevelInterval > 0 && (CompletedLevelNumber % MilestoneLevelInterval) == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("[ASAGameState] %d단계 단위 마일스톤 레벨 %d 클리어! 연출 후 WIN 결과 화면을 보여줍니다."), MilestoneLevelInterval, CompletedLevelNumber);
		PlayLevelExitWipeAndShowResult(ESAResultType::Win);
		return;
	}

	const USATileSettings* Settings = USATileSettings::Get();
	const int32 TotalLevels = Settings ? Settings->GetTotalLevelCount() : 50;

	// 마지막 레벨을 클리어한 경우: 그대로 멈춘 상태 유지
	if (CompletedLevelNumber >= TotalLevels)
	{
		UE_LOG(LogTemp, Display, TEXT("[ASAGameState] 마지막 레벨 %d 클리어 완료! 게임 완료 상태를 유지합니다."), CompletedLevelNumber);
		return;
	}

	const int32 NextLevelNum = CompletedLevelNumber + 1;
	UE_LOG(LogTemp, Display, TEXT("[ASAGameState] 레벨 %d 클리어! 바로 다음 레벨 %d 로 전환합니다."), CompletedLevelNumber, NextLevelNum);

	if (NextLevelTransitionDelay > 0.0f)
	{
		FTimerHandle NextLevelTimerHandle;
		FTimerDelegate TimerDel;
		TimerDel.BindUObject(this, &ASAGameState::StartGame, NextLevelNum);
		GetWorldTimerManager().SetTimer(NextLevelTimerHandle, TimerDel, NextLevelTransitionDelay, false);
	}
	else
	{
		StartGame(NextLevelNum);
	}
}

void ASAGameState::OnMenuLevelSelected(int32 SelectedLevelNumber)
{
	// SAMenuManager는 등록된 레벨이 있는 타일에서만 이 이벤트를 발생시키지만, 혹시 모를 상황에 대비해 한 번 더 확인
	const USATileSettings* Settings = USATileSettings::Get();
	FSATileLevelData LevelData;
	if (!Settings || !Settings->GetLevelData(SelectedLevelNumber, LevelData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ASAGameState] 메뉴에서 선택된 레벨 %d 데이터를 찾을 수 없어 진행할 수 없습니다."), SelectedLevelNumber);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ASAGameState] 메뉴에서 레벨 %d 선택됨! Game(Ingame) 상태로 전환합니다."), SelectedLevelNumber);
	StartGame(SelectedLevelNumber);
}

void ASAGameState::HandleTrapTriggered()
{
	UE_LOG(LogTemp, Display, TEXT("[ASAGameState] 함정에 걸렸습니다! 연출 후 LOSE 결과 화면을 보여줍니다."));
	PlayLevelExitWipeAndShowResult(ESAResultType::Lose);
}

void ASAGameState::HandleMonsterCaughtPlayer()
{
	UE_LOG(LogTemp, Display, TEXT("[ASAGameState] 몬스터에게 붙잡혔습니다! 연출 후 LOSE 결과 화면을 보여줍니다."));
	PlayLevelExitWipeAndShowResult(ESAResultType::Lose);
}

void ASAGameState::HandleMenuButtonSwiped()
{
	UE_LOG(LogTemp, Log, TEXT("[ASAGameState] 화면 우상단 메뉴 이동 슬라이더 스와이프! 메뉴로 전환합니다."));
	OpenMenu();
}

void ASAGameState::HandleTileWipeCompleted()
{
	ShowResult(PendingResultType);
}

void ASAGameState::HandleResultUnlockSwiped()
{
	UE_LOG(LogTemp, Log, TEXT("[ASAGameState] 결과 화면 슬라이더 잠금 해제! 메뉴로 전환합니다."));
	OpenMenu();
}

void ASAGameState::PlayLevelExitWipeAndShowResult(ESAResultType InResultType)
{
	PendingResultType = InResultType;

	// 동물/음식 액터 및 트랩 이펙트 풀을 전부 완전히 삭제
	if (USAActorManager* ActorManager = USAActorManager::Get(this))
	{
		ActorManager->ClearManagedActors();
		ActorManager->DestroyTrapEffectPool();
	}

	// 타일이 바깥에서 안쪽으로 사라지는 와이프 연출 시작 (완료되면 HandleTileWipeCompleted가 결과 화면으로 전환)
	if (USATileManager* TileManager = USATileManager::Get(this))
	{
		TileManager->StartTileWipeEffect();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ASAGameState] SATileManager 서브시스템을 찾을 수 없어 연출 없이 바로 결과 화면으로 전환합니다."));
		ShowResult(PendingResultType);
	}
}
