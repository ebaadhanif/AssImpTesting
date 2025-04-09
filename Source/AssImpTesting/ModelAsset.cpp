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
    TArray<FString> FoundModelFiles;
    FString BasePath = ModelsFolderpath;
    FPaths::NormalizeDirectoryName(BasePath); // Normalize path (slashes)

    if (!FPaths::DirectoryExists(BasePath))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Models folder does not exist: %s"), *BasePath);
        return;
    }

    TArray<FString> Extensions = { TEXT("*.fbx"), TEXT("*.glb"), TEXT("*.obj"), TEXT("*.dae"), TEXT("*.3ds"), TEXT("*.stl") };

    for (const FString& Ext : Extensions)
    {
        TArray<FString> TempFiles;
        IFileManager::Get().FindFilesRecursive(TempFiles, *BasePath, *Ext, true, false);
        FoundModelFiles.Append(TempFiles);
    }

    // Final list of valid model paths
    for (const FString& FilePath : FoundModelFiles)
    {
        UE_LOG(LogTemp, Display, TEXT("✅ Found model: %s"), *FilePath);
        Initialize3DModel(FilePath);
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
