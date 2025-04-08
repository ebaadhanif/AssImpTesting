#include "ModelAsset.h"

AModelAsset::AModelAsset()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    ConfigManager = CreateDefaultSubobject<UModelsConfigManager>(TEXT("ConfigManager"));

}

void AModelAsset::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

void AModelAsset::BeginPlay()
{
    Super::BeginPlay();
    ConfigManager->LoadConfig(ModelsConfigFilepath); 
    TArray<FString> FbxFiles, GlbFiles;
    IFileManager::Get().FindFiles(FbxFiles, *(ModelsFolderpath / TEXT("*.fbx")), true, false);
    IFileManager::Get().FindFiles(GlbFiles, *(ModelsFolderpath / TEXT("*.glb")), true, false);
    for (const FString& File : FbxFiles)
    {
        FString Path = FPaths::Combine(ModelsFolderpath, File);
        Initialize3DModel(Path);
    }
    for (const FString& File : GlbFiles)
    {
        FString Path = FPaths::Combine(ModelsFolderpath, File);
        Initialize3DModel(Path);
    }
    for (UAssimpRuntime3DModelsImporter* Model : Loaded3DModels)
    {
        SpawnAndConfigure3DModel(Model, FVector::ZeroVector);
    }
}

void AModelAsset::Initialize3DModel(FString Path)
{
    UAssimpRuntime3DModelsImporter* Model = NewObject<UAssimpRuntime3DModelsImporter>(this);
    if (Model)
    {
        Model->LoadAssimpDLLIfNeeded();
        Model->LoadFBXModel(Path);
        Model->SetModelName(ExtractModelNameFromPath(Path));
        Loaded3DModels.Add(Model);
    }
}

void AModelAsset::SpawnAndConfigure3DModel(UAssimpRuntime3DModelsImporter* Model, const FVector& SpawnLocation)
{
    Model->SpawnModel(GetWorld(), SpawnLocation);
    if (ConfigManager)
    {
        ConfigManager->AttachConfigToModel(Model);
    }
}

FString AModelAsset::ExtractModelNameFromPath(const FString& Path)
{
    return FPaths::GetBaseFilename(Path);
}
