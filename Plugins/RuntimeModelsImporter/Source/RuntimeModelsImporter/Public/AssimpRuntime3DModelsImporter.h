// Class Added by Ebaad, This class deals with Model Raw Data Extraction using Assimp , Creating Model From that Data and Spawning on Demand in Scene On Certain Location
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
    void LoadAssimpDLLIfNeeded();
    void ImportModel(const FString& InFilePath);
    void SetModelID(const FString& InID) { ModelID = InID; }
    FString GetModelID() const { return ModelID; }
    void SetModelName(const FString& InName) { ModelName = InName; }
    FString GetModelName() const { return ModelName; }
    AActor* SpawnModel(UWorld* World, const FTransform& modelTransform);
    void ApplyTransform(const FTransform& modelTransform);
    void DebugAllTexturesInScene(const aiScene* Scene, const FString& InFilePath);
    FString GetTextureTypeName(aiTextureType Type);
    void HideModel();
    AActor* GetNodeActorByName(const FString& NodeName) const; // for attaching config
private:
    const FModelNodeData& GetRootNode() const { return RootNode; }
    void ParseNode(aiNode* Node, const aiScene* Scene, FModelNodeData& OutNode, const FString& FbxFilePath);
    void ExtractMesh(aiMesh* Mesh, const aiScene* Scene, FModelMeshData& OutMesh, const FString& FbxFilePath);
    void SpawnNodeRecursive(UWorld* World,const FModelNodeData& Node, AActor* Parent);
    FTransform ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix);
    void LoadMasterMaterial();
    bool IsVectorFinite(const FVector& Vec);
    bool IsTransformValid(const FTransform& Transform);
    TMap<FString, AActor*> SpawnedNodeActors;
    UMaterialInstanceDynamic* CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene, const FString& FbxFilePath);
    UTexture2D* CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName, aiTextureType Type, const FString& MaterialName, const FName& ParamName);
    UTexture2D* LoadTextureFromDisk(const FString& TexturePath, const FString& MaterialName, const FName& ParamName, aiTextureType Type);
    UTexture2D* LoadDDSTexture(const FString& DDSTexture, aiTextureType Type);
    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> LoadedMaterials;
    UPROPERTY()
    UMaterial* MasterMaterial = nullptr;
    AActor* RootFBXActor = nullptr;
    FString ModelID = "DefaultModelID";
    FString ModelName = "DefaultModelName";
    FString FilePath;
    FModelNodeData RootNode;
    TMap<aiMaterial*, UMaterialInstanceDynamic*> MaterialCache;

    const aiScene* TempScene;
};

