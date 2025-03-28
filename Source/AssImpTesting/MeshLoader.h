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

    UFUNCTION(BlueprintCallable, Category = "FBX Import")
    void LoadFBXModel(const FString& FilePath);

    void ProcessNode(aiNode* Node, const aiScene* Scene, AActor* ParentActor, const FString& FilePath);

protected:
    virtual void BeginPlay() override;

private:
  
    
    void ProcessMesh(aiMesh* Mesh, const aiScene* Scene, AActor* NodeActor, const FString& FilePath, const FTransform& LocalTransform);
    void CreateProceduralMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs, const TArray<FVector>& Tangents, const TArray<FVector>& Bitangents, UMaterialInterface* Material, AActor* OwnerActor, const FTransform& LocalTransform, const FString& MeshName);
    
    AActor* CreateNodeActor(aiNode* Node, const aiMatrix4x4& TransformMatrix, AActor* ParentActor);
    FTransform ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix);
    

    void LoadMasterMaterial();
    bool IsVectorFinite(const FVector& Vec);
    bool IsTransformValid(const FTransform& Transform);
    UTexture2D* LoadTextureFromDisk(const FString& FilePath);
    UTexture2D* CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName);
    UMaterialInstanceDynamic* CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene, const FString& FilePath);

private:
    UPROPERTY()
    TArray<UMaterialInstanceDynamic*> LoadedMaterials;

    UPROPERTY()
    UMaterial* MasterMaterial = nullptr;

    AActor* RootFBXActor = nullptr;
};