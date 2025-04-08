// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "assimp/scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/TextureDefines.h"
#include "Materials/Material.h"
#include "TextureResource.h"         // For PlatformData
#include "Rendering/Texture2DResource.h"
#include "AssimpRuntime3DModelsImporter.generated.h"
struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;
struct aiTexture;

// --- Mesh Section Info
USTRUCT()
struct FModelMeshData
{
    GENERATED_BODY()

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FVector> Tangents;
    TArray<FVector> Bitangents;
    FString MaterialName;
    UMaterialInterface* Material = nullptr;

};

// --- Node
USTRUCT()
struct FModelNodeData
{
    GENERATED_BODY()

    FString Name;
    FTransform Transform;
    TArray<FModelNodeData> Children;
    TArray<FModelMeshData> MeshSections;
};
UCLASS()
class RUNTIMEMODELSIMPORTER_API UAssimpRuntime3DModelsImporter : public UObject
{
    GENERATED_BODY()

public:
    UAssimpRuntime3DModelsImporter();
    void LoadFBXModel(const FString& FbxFilePath);
    void ParseNode(aiNode* Node, const aiScene* Scene, FModelNodeData& OutNode, const FString& FbxFilePath);
    AActor* SpawnModel(UWorld* World, const FVector& SpawnLocation);
    const FModelNodeData& GetRootNode() const { return RootNode; }
    void LoadAssimpDLLIfNeeded();
    AActor* GetNodeActorByName(const FString& NodeName) const;
    void SetModelID(const FString& InID) { ModelID = InID; }
    FString GetModelID() const { return ModelID; }
    void SetModelName(const FString& InName) { ModelName = InName; }
    FString GetModelName() const { return ModelName; }

private:
    void ExtractMesh(aiMesh* Mesh, const aiScene* Scene, FModelMeshData& OutMesh, const FString& FbxFilePath);
    //UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Model")
    void SpawnNodeRecursive(const FModelNodeData& Node, AActor* Parent);
    FTransform ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix);
    void LoadMasterMaterial();
    bool IsVectorFinite(const FVector& Vec);
    bool IsTransformValid(const FTransform& Transform);
    TMap<FString, AActor*> SpawnedNodeActors;

    UTexture2D* LoadTextureFromDisk(const FString& FbxFilePath);
    UMaterialInstanceDynamic* CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene, const FString& FbxFilePath);
    UTexture2D* CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName, aiTextureType Type);
    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> LoadedMaterials;
    UPROPERTY()
    UMaterial* MasterMaterial = nullptr;
    AActor* RootFBXActor = nullptr;
    FString ModelID = "DefaultModelID";
    FString ModelName = "DefaultModelName";
    FString FilePath;
    FModelNodeData RootNode;

};