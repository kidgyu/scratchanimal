// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SATileLevelData.h"
#include "SATileSettings.generated.h"

/**
 * 타일 게임의 레벨(1 ~ 50) 데이터 및 전역 타일 설정을 관리하는 DeveloperSettings 클래스
 * 프로젝트 세팅(Project Settings -> Game -> SA Tile Settings)에서 확인 및 편집 가능
 */
UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "SA Tile Settings"))
class SCRATCHANIMAL_API USATileSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USATileSettings();

	/** 프로젝트 세팅 카테고리 설정 */
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }
	virtual FName GetSectionName() const override { return FName(TEXT("SATileSettings")); }

	/** 레벨 1부터 50까지의 타일 배치 및 규격 데이터 목록 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tile Levels", meta = (TitleProperty = "LevelNumber"))
	TArray<FSATileLevelData> LevelDataList;

	/** 싱글톤처럼 어디서나 접근 가능한 USATileSettings 인스턴스 획득 */
	UFUNCTION(BlueprintPure, Category = "Tile Settings")
	static const USATileSettings* Get();

	/** 특정 레벨의 데이터를 검색 (레벨 1 ~ 50) */
	UFUNCTION(BlueprintCallable, Category = "Tile Settings")
	bool GetLevelData(int32 InLevelNumber, FSATileLevelData& OutLevelData) const;

	/** 총 레벨 수 반환 */
	UFUNCTION(BlueprintPure, Category = "Tile Settings")
	int32 GetTotalLevelCount() const
	{
		if (LevelDataList.IsEmpty())
		{
			const_cast<USATileSettings*>(this)->GenerateDefaultLevels();
		}
		return LevelDataList.Num();
	}

	/** 1부터 50까지의 기본 레벨 디자인 데이터를 생성 및 재설정 */
	UFUNCTION(BlueprintCallable, Category = "Tile Settings")
	void GenerateDefaultLevels();
};
