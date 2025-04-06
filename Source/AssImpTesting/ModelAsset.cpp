// Fill out your copyright notice in the Description page of Project Settings.


#include "ModelAsset.h"


// Sets default values
AModelAsset::AModelAsset()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    ConfigManager = CreateDefaultSubobject<UModelsConfigManager>(TEXT("ConfigManager"));

}

// Called every frame
void AModelAsset::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

void AModelAsset::BeginPlay()
{
    Super::BeginPlay();

    ConfigManager->LoadConfig(ModelsConfigFilepath); // Reads full config for all models



    TArray<FString> FbxFiles, GlbFiles;
    IFileManager::Get().FindFiles(FbxFiles, *(ModelsFolderpath / TEXT("*.fbx")), true, false);
    IFileManager::Get().FindFiles(GlbFiles, *(ModelsFolderpath / TEXT("*.glb")), true, false);

    for (const FString& File : FbxFiles)
    {
        FString Path = FPaths::Combine(ModelsFolderpath, File);
        AMeshLoader* Loader = GetWorld()->SpawnActor<AMeshLoader>();
        if (Loader)
        {
            Loader->LoadAssimpDLLIfNeeded();
            Loader->LoadFBXModel(Path);
            Loader->SetModelName(ExtractModelNameFromPath(Path));
            Loader->SpawnModel(GetWorld(), FVector::ZeroVector);
            LoadedModels.Add(Loader);

            if (ConfigManager)
            {
                ConfigManager->AttachConfigToModel(Loader);
            }
        }
    }
    for (const FString& File : GlbFiles)
    {
        FString Path = FPaths::Combine(ModelsFolderpath, File);
        AMeshLoader* Loader = GetWorld()->SpawnActor<AMeshLoader>();
        if (Loader)
        {
            Loader->LoadAssimpDLLIfNeeded();
            Loader->LoadFBXModel(Path);
            Loader->SetModelName(ExtractModelNameFromPath(Path));
            Loader->SpawnModel(GetWorld(), FVector::ZeroVector);
            LoadedModels.Add(Loader);

            if (ConfigManager)
            {
                ConfigManager->AttachConfigToModel(Loader);
            }
        }
    }

}


void AModelAsset::SetConfigManager(UModelsConfigManager* InConfigManager)
{
    ConfigManager = InConfigManager;
}


FString AModelAsset::ExtractModelNameFromPath(const FString& Path)
{
    // Customize this if ID is based on something else
    return FPaths::GetBaseFilename(Path);
}




        