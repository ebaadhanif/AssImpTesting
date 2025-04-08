// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AssimpRuntime3DModelsImporter.h"
#include "ModelsConfigManager.generated.h"

USTRUCT()
struct FAttachmentConfig
{
    GENERATED_BODY()

    UPROPERTY()
    FString NodeName;

    UPROPERTY()
    FString AttachmentType; // StaticMesh, VFX, Blueprint

    UPROPERTY()
    FString AssetPath;
};

USTRUCT()
struct FModelAttachmentConfig
{
    GENERATED_BODY()

    UPROPERTY()
    FString ModelName;

    UPROPERTY()
    FString ModelID;

    UPROPERTY()
    TArray<FAttachmentConfig> Attachments;
};


UCLASS()
class ASSIMPTESTING_API UModelsConfigManager : public UObject
{
    GENERATED_BODY()

public:
    void LoadConfig(FString FilePath);
    void AttachConfigToModel(UAssimpRuntime3DModelsImporter* Loader);

private:
    TArray<FModelAttachmentConfig> ModelConfigs;
    void AttachElementToNode(const FAttachmentConfig& Attachment, AActor* NodeActor);
};