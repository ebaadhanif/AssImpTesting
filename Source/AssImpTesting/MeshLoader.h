#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "MeshLoader.generated.h"

UCLASS()
class ASSIMPTESTING_API AMeshLoader : public AActor
{
    GENERATED_BODY()

public:
    AMeshLoader();

    UPROPERTY()
    TArray<UStaticMesh*> LoadedMeshes;

    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> LoadedMaterials;

    UFUNCTION(BlueprintCallable, Category = "FBX Import")
    void LoadFBXModel(const FString& FilePath);

protected:
    virtual void BeginPlay() override; 

private:
    UPROPERTY()
    UStaticMeshComponent* StaticMeshComponent;

    void ProcessNode(aiNode* Node, const aiScene* Scene, const FTransform& ParentTransform);
    void ProcessMesh(aiMesh* Mesh, const aiScene* Scene, const FTransform& Transform);
    UStaticMesh* CreateStaticMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs, const TArray<FVector>& Tangents, const TArray<FVector>& Bitangents);
   // UStaticMesh* CreateStaticMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs);
    void SpawnMeshInScene(UStaticMesh* StaticMesh, const FTransform& Transform);
    FTransform ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix);
    UMaterialInstanceDynamic* CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene);
    UTexture2D* CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName);
    FString ResolveTexturePath(const FString& TexturePath);
    UTexture2D* LoadTextureFromFile(const FString& FilePath);

private:
    // Replace ConstructorHelpers with lazy loading
    UPROPERTY()
    UMaterial* MasterMaterial;

    void LoadMasterMaterial();

    UTexture2D* LoadTextureFromDisk(const FString& FilePath);

    FString FBXFilePath= ("C:/Users/ebaad.hanif/Desktop/FBX Models/car1.fbx");;
};