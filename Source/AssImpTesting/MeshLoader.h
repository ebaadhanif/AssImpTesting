#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "assimp/scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "MeshLoader.generated.h"

UCLASS()
class ASSIMPTESTING_API AMeshLoader : public AActor
{
    GENERATED_BODY()

public:
    AMeshLoader();

protected:
    virtual void BeginPlay() override;

public:
    UFUNCTION(BlueprintCallable, Category = "FBX Import")
    void LoadFBXModel(const FString& FilePath);

private:
    void ProcessNode(aiNode* Node, const aiScene* Scene, const FTransform& ParentTransform);
    void ProcessMesh(aiMesh* Mesh, const aiScene* Scene, const FTransform& Transform);

    UStaticMesh* CreateStaticMesh(
        const TArray<FVector>& Vertices,
        const TArray<int32>& Triangles,
        const TArray<FVector>& Normals,
        const TArray<FVector2D>& UVs,
        const TArray<FVector>& Tangents,
        const TArray<FVector>& Bitangents);

    FTransform ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix);
    UMaterialInstanceDynamic* CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene);
    UTexture2D* CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName);

    FString ResolveTexturePath(const FString& TexturePath);
    UTexture2D* LoadTextureFromFile(const FString& FullPath);
    UTexture2D* LoadTextureFromDisk(const FString& FilePath);
    void LoadMasterMaterial();

private:
    UPROPERTY()
    UStaticMeshComponent* StaticMeshComponent;

    UPROPERTY()
    UMaterial* MasterMaterial;

    UPROPERTY()
    TArray<UStaticMesh*> LoadedMeshes;

    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> LoadedMaterials;

    UPROPERTY(EditAnywhere, Category = "FBX Import")
    FString FBXFilePath = TEXT("C:/Users/ebaad.hanif/Desktop/FBX Models/Car.fbx");
};