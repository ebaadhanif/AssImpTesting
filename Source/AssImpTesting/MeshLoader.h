#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "assimp/scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "MeshLoader.generated.h"

struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;
struct aiTexture;

// --- Mesh Section Info
USTRUCT()
struct FMeshSectionData
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
struct FFBXNodeData
{
    GENERATED_BODY()

    FString Name;
    FTransform Transform;
    TArray<FFBXNodeData> Children;
    TArray<FMeshSectionData> MeshSections;
};


UCLASS()
class ASSIMPTESTING_API AMeshLoader : public AActor
{
    GENERATED_BODY()

public:
    AMeshLoader();
    void LoadFBXModel(const FString& FbxFilePath);
    void ParseNode(aiNode* Node, const aiScene* Scene, FFBXNodeData& OutNode, const FString& FbxFilePath);
    AActor* SpawnModel(UWorld* World, const FVector& SpawnLocation);
    const FString& GetModelName() const { return ModelName; }
    const FString& GetFilePath() const { return FilePath; }
    const FFBXNodeData& GetRootNode() const { return RootNode; }

private:
    void ExtractMesh(aiMesh* Mesh, const aiScene* Scene, FMeshSectionData& OutMesh, const FString& FbxFilePath);

    void SpawnNodeRecursive(const FFBXNodeData& Node, AActor* Parent);
    FTransform ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix);
    void LoadMasterMaterial();
    bool IsVectorFinite(const FVector& Vec);
    bool IsTransformValid(const FTransform& Transform);
    UTexture2D* LoadTextureFromDisk(const FString& FbxFilePath);
    UMaterialInstanceDynamic* CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene, const FString& FbxFilePath);
    UTexture2D* CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName, aiTextureType Type);
    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> LoadedMaterials;
    UPROPERTY()
    UMaterial* MasterMaterial = nullptr;
    AActor* RootFBXActor = nullptr;
    FString ModelName;
    FString FilePath;
    FFBXNodeData RootNode;

};