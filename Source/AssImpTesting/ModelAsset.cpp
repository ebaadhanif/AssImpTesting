// Fill out your copyright notice in the Description page of Project Settings.


#include "ModelAsset.h"
#include "MeshLoader.h"

// Sets default values
AModelAsset::AModelAsset()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called every frame
void AModelAsset::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AModelAsset::BeginPlay() 
{
    Super::BeginPlay();
    AMeshLoader* Loader = GetWorld()->SpawnActor<AMeshLoader>();
    Loader->LoadAssimpDLLIfNeeded();
    
    TArray<FString> FbxFiles, GlbFiles;
    IFileManager::Get().FindFiles(FbxFiles, *(ModelsFolderpath / TEXT("*.fbx")), true, false);
    IFileManager::Get().FindFiles(GlbFiles, *(ModelsFolderpath / TEXT("*.glb")), true, false);

    for (const FString& File : FbxFiles)
    {
        Loader->LoadFBXModel(FPaths::Combine(ModelsFolderpath, File));
        Loader->SpawnModel(GetWorld(), FVector(0, 0, 0));
    }
    for (const FString& File : GlbFiles) 
    {
        Loader->LoadFBXModel(FPaths::Combine(ModelsFolderpath, File));
        Loader->SpawnModel(GetWorld(), FVector(0, 0, 200));
    }

}

