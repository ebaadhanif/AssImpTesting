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

    UFUNCTION(BlueprintCallable, Category = "FBX Import")
    void LoadFBXFilesFromFolder(const FString& FbxFolderPath);

    UPROPERTY()
    TArray<UStaticMesh*> LoadedMeshes;

    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> LoadedMaterials;

    UFUNCTION(BlueprintCallable, Category = "FBX Import")
    void LoadFBXModel(const FString& FilePath);

    void ProcessNode(aiNode* Node, const aiScene* Scene, const FTransform& ParentTransform, const FString& FilePath);

    void ProcessMesh(aiMesh* Mesh, const aiScene* Scene, const FTransform& Transform, const FString& FilePath);

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    UStaticMeshComponent* StaticMeshComponent;
    void CreateProceduralMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs, const TArray<FVector>& Tangents, const TArray<FVector>& Bitangents, UMaterialInterface* Material, const FTransform& Transform);
    FTransform ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix);
    UTexture2D* CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName);
    UTexture2D* LoadTextureFromDisk(const FString& FilePath);
    UMaterialInstanceDynamic* CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene, const FString& FilePath);
    void LoadMasterMaterial();
    UMaterial* MasterMaterial = nullptr;
};