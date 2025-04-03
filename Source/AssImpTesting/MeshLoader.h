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
    UMaterialInterface* Material = nullptr; // ✅ Add this line

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

// --- Full Model
USTRUCT()
struct FFBXModelData
{
    GENERATED_BODY()

    FString ModelName;
    FString FilePath;
    FFBXNodeData RootNode;
};


UCLASS()
class ASSIMPTESTING_API AMeshLoader : public AActor
{
    GENERATED_BODY()

public:
    AMeshLoader();
    UFUNCTION(BlueprintCallable, Category = "FBX Import")
    void Load_FBXAndGLB_ModelFilesFromFolder(const FString& Folder);
    UFUNCTION(BlueprintCallable, Category = "FBX Import")
    void LoadFBXModel(const FString& FilePath);
    void ParseNode(aiNode* Node, const aiScene* Scene, FFBXNodeData& OutNode, const FString& FilePath);

protected:
    virtual void BeginPlay() override;
private:
    void ExtractMesh(aiMesh* Mesh, const aiScene* Scene, FMeshSectionData& OutMesh, const FString& FilePath);
    void SpawnCachedModels();
    void SpawnNodeRecursive(const FFBXNodeData& Node, AActor* Parent);
    FTransform ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix);
     void LoadMasterMaterial();
    bool IsVectorFinite(const FVector& Vec);
    bool IsTransformValid(const FTransform& Transform);
    const TArray<FFBXModelData>& GetCachedModels();
    UTexture2D* LoadTextureFromDisk(const FString& FilePath);
    UTexture2D* CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName);
    UMaterialInstanceDynamic* CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene, const FString& FilePath);
private:
    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> LoadedMaterials;
    UPROPERTY()
    UMaterial* MasterMaterial = nullptr;
    AActor* RootFBXActor = nullptr;
    TArray<FFBXModelData> CachedModels;
};