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
       // FVector location = FVector(100, 100, 100);    
        FVector location = FVector(336890.000000, -438060.000000, -30100.000000);
        FRotator rotation = FRotator(0, 0, 0);                    
        FVector scale = FVector(1, 1, 1);                         
        FTransform modelTransform = FTransform(rotation, location, scale);
        Model->SpawnModel(GetWorld(), modelTransform);
         modelTransform = FTransform(rotation, FVector(100, 100, 100), scale);
        Model->SpawnModel(GetWorld(), modelTransform);
       // Model->HideModel(); 
    }
}

void AModelAsset::Initialize3DModel(FString Path)
{
    UAssimpRuntime3DModelsImporter* Model = NewObject<UAssimpRuntime3DModelsImporter>(this);
    if (Model)
    {
        Model->LoadAssimpDLLIfNeeded();
        Model->ImportModel(Path);
        Model->SetModelName(ExtractModelNameFromPath(Path));
        Loaded3DModels.Add(Model);
    }
}


FString AModelAsset::ExtractModelNameFromPath(const FString& Path)
{
    return FPaths::GetBaseFilename(Path);
}



