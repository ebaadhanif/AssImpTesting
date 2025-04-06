// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeshLoader.h"
#include "ModelsConfigManager.h"
#include "ModelAsset.generated.h"

UCLASS()
class ASSIMPTESTING_API AModelAsset : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AModelAsset();
	FString ModelsFolderpath = "C:/Users/ebaad/OneDrive/Desktop/FBX Models";
	FString ModelsConfigFilepath = FPaths::ProjectContentDir() / TEXT("Archive/ModelsConfig.json");

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
public:
	void SetConfigManager(UModelsConfigManager* InConfigManager);
private:
	TArray<AMeshLoader*> LoadedModels;
	UPROPERTY()
	UModelsConfigManager* ConfigManager;

	FString ExtractModelNameFromPath(const FString& Path);
};
