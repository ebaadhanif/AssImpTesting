#include "MeshLoader.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "StaticMeshAttributes.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#include "Engine/StaticMeshActor.h"
#include "PhysicsEngine/BodySetup.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Materials/MaterialInstanceDynamic.h"

AMeshLoader::AMeshLoader() {
    PrimaryActorTick.bCanEverTick = false;
}

void AMeshLoader::LoadFBXModel(const FString& InFilePath)
{
    FilePath = InFilePath;
    ModelName = FPaths::GetBaseFilename(FilePath);

    Assimp::Importer Importer;
    const aiScene* Scene = Importer.ReadFile(TCHAR_TO_UTF8(*FilePath),
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_FlipUVs);

    if (!Scene || !Scene->mRootNode)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to load FBX: %s"), *FilePath);
        return;
    }

    RootNode = FFBXNodeData();
    ParseNode(Scene->mRootNode, Scene, RootNode, FilePath);
}

void AMeshLoader::ParseNode(aiNode* Node, const aiScene* Scene, FFBXNodeData& OutNode, const FString& FbxFilePath) {
    OutNode.Name = UTF8_TO_TCHAR(Node->mName.C_Str());
    OutNode.Transform = ConvertAssimpMatrix(Node->mTransformation);
    if (FbxFilePath.EndsWith(".glb") || FbxFilePath.EndsWith(".gltf"))
    {
        OutNode.Transform.SetScale3D(FVector(100.0f)); // adjust accordingly
    }


    for (uint32 i = 0; i < Node->mNumMeshes; ++i) {
        aiMesh* Mesh = Scene->mMeshes[Node->mMeshes[i]];
        FMeshSectionData MeshData;
        ExtractMesh(Mesh, Scene, MeshData, FbxFilePath);
        OutNode.MeshSections.Add(MoveTemp(MeshData));
    }

    for (uint32 i = 0; i < Node->mNumChildren; ++i) {
        FFBXNodeData ChildNode;
        ParseNode(Node->mChildren[i], Scene, ChildNode, FbxFilePath);
        OutNode.Children.Add(MoveTemp(ChildNode));
    }
}

void AMeshLoader::ExtractMesh(aiMesh* Mesh, const aiScene* Scene, FMeshSectionData& OutMesh, const FString& FbxFilePath) {
    for (uint32 i = 0; i < Mesh->mNumVertices; ++i) {
        OutMesh.Vertices.Add(FVector(Mesh->mVertices[i].x, Mesh->mVertices[i].z, Mesh->mVertices[i].y));
        OutMesh.Normals.Add(FVector(Mesh->mNormals[i].x, Mesh->mNormals[i].z, Mesh->mNormals[i].y));
        OutMesh.UVs.Add(Mesh->HasTextureCoords(0) ? FVector2D(Mesh->mTextureCoords[0][i].x, Mesh->mTextureCoords[0][i].y) : FVector2D::ZeroVector);
    }
    for (uint32 i = 0; i < Mesh->mNumFaces; ++i) {
        const aiFace& Face = Mesh->mFaces[i];
        if (Face.mNumIndices == 3) {
            OutMesh.Triangles.Add(Face.mIndices[0]);
            OutMesh.Triangles.Add(Face.mIndices[1]);
            OutMesh.Triangles.Add(Face.mIndices[2]);
        }
    }
    if (Mesh->mMaterialIndex >= 0 && Scene->mMaterials[Mesh->mMaterialIndex]) {
        OutMesh.Material = CreateMaterialFromAssimp(Scene->mMaterials[Mesh->mMaterialIndex], Scene, FbxFilePath);
    }
}

AActor* AMeshLoader::SpawnModel(UWorld* World, const FVector& SpawnLocation)
{
    if (!World) return nullptr;

    // Spawn root actor
    AActor* RootActor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnLocation, FRotator::ZeroRotator);
    RootActor = RootActor;

#if WITH_EDITOR
    RootActor->SetActorLabel(ModelName);
#endif

    USceneComponent* RootComp = NewObject<USceneComponent>(RootActor);
    RootComp->RegisterComponent();
    RootActor->SetRootComponent(RootComp);

    for (FFBXNodeData& Child : RootNode.Children)
    {
        SpawnNodeRecursive(Child, RootActor);
    }

    return RootActor;
}


void AMeshLoader::SpawnNodeRecursive(const FFBXNodeData& Node, AActor* Parent) {
    AActor* NodeActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
#if WITH_EDITOR
    NodeActor->SetActorLabel(Node.Name);
#endif
    USceneComponent* RootComp = NewObject<USceneComponent>(NodeActor);
    RootComp->RegisterComponent();
    NodeActor->SetRootComponent(RootComp);
    NodeActor->AttachToActor(Parent, FAttachmentTransformRules::KeepRelativeTransform);
    NodeActor->SetActorRelativeTransform(Node.Transform);

    for (const FMeshSectionData& Section : Node.MeshSections) {
        UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(NodeActor);
        Mesh->RegisterComponent();
        Mesh->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
        TArray<FProcMeshTangent> Tangents;
        for (const FVector& Tangent : Section.Tangents) {
            Tangents.Add(FProcMeshTangent(Tangent, false));
        }
        Mesh->CreateMeshSection_LinearColor(0, Section.Vertices, Section.Triangles, Section.Normals, Section.UVs, {}, Tangents, true);
        Mesh->SetMaterial(0, Section.Material ? Section.Material : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
    }
    for (const FFBXNodeData& Child : Node.Children) {
        SpawnNodeRecursive(Child, NodeActor);
    }
}

FTransform AMeshLoader::ConvertAssimpMatrix(const aiMatrix4x4& M) {
    FMatrix Matrix(
        FPlane(M.a1, M.a2, M.a3, M.a4),
        FPlane(M.b1, M.b2, M.b3, M.b4),
        FPlane(M.c1, M.c2, M.c3, M.c4),
        FPlane(M.d1, M.d2, M.d3, M.d4));
    return FTransform(FQuat(Matrix.Rotator()), FVector(Matrix.M[3][0], Matrix.M[3][1], Matrix.M[3][2]), Matrix.GetScaleVector());
}

void AMeshLoader::LoadMasterMaterial() {
    MasterMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_BaseMaterial.M_BaseMaterial"));
    if (!MasterMaterial) {
        MasterMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }
}

UMaterialInstanceDynamic* AMeshLoader::CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene, const FString& FbxFilePath)
{
    if (!MasterMaterial)
        LoadMasterMaterial();

    if (!MasterMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No valid master material loaded."));
        return nullptr;
    }

    UMaterialInstanceDynamic* MatInstance = UMaterialInstanceDynamic::Create(MasterMaterial, GetTransientPackage());
    if (!MatInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create dynamic material instance."));
        return nullptr;
    }

    LoadedMaterials.Add(MatInstance);
    const FString BaseDir = FPaths::GetPath(FbxFilePath);

    // Handle fallback base color
    aiColor3D DiffuseColor(1.0f, 1.0f, 1.0f);
    if (AssimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, DiffuseColor) == AI_SUCCESS)
    {
        FLinearColor Color(DiffuseColor.r, DiffuseColor.g, DiffuseColor.b);
        MatInstance->SetVectorParameterValue("BaseColor", Color);
        UE_LOG(LogTemp, Warning, TEXT("🎨 Fallback color set: R=%.2f G=%.2f B=%.2f"), Color.R, Color.G, Color.B);
    }

    auto TryApplyTexture = [&](aiTextureType Type, const FName& ParamName, const FName& EnableParamName)
        {
            aiString TexPath;
            if (AssimpMaterial->GetTexture(Type, 0, &TexPath) == AI_SUCCESS)
            {
                FString Path = UTF8_TO_TCHAR(TexPath.C_Str());
                UTexture2D* Texture = nullptr;

                if (Path.StartsWith(TEXT("*")))
                {
                    const aiTexture* Embedded = Scene->GetEmbeddedTexture(TexPath.C_Str());
                    Texture = CreateTextureFromEmbedded(Embedded, Path, Type);
                }
                else
                {

                    FString FullPath = FPaths::Combine(BaseDir, Path);
                    FPaths::NormalizeFilename(FullPath);  // ✅ fix mixed slashes
                    if (!FPaths::FileExists(FullPath))
                    {
                        UE_LOG(LogTemp, Warning, TEXT("❌ Texture file not found: %s"), *FullPath);
                    }
                    Texture = LoadTextureFromDisk(FullPath);
                }

                if (Texture)
                {
                    MatInstance->SetTextureParameterValue(ParamName, Texture);
                    if (!EnableParamName.IsNone())
                        MatInstance->SetScalarParameterValue(EnableParamName, 1.0f);
                    UE_LOG(LogTemp, Display, TEXT("✅ Applied texture: %s → %s"), *Path, *ParamName.ToString());
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("❌ Texture not found: %s for %s"), *Path, *ParamName.ToString());
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("⚠️ No texture assigned in FBX for type: %d (%s)"), (int32)Type, *ParamName.ToString());
            }
        };

    // Try all known textures with mapping to master material
    TryApplyTexture(aiTextureType_DIFFUSE, "BaseColorTex", "UseBaseColorTex");
    TryApplyTexture(aiTextureType_BASE_COLOR, "BaseColorTex", "UseBaseColorTex");
    TryApplyTexture(aiTextureType_NORMALS, "NormalMap", "UseNormalMap");
    TryApplyTexture(aiTextureType_HEIGHT, "NormalMap", "UseNormalMap"); // fallback
    TryApplyTexture(aiTextureType_SPECULAR, "SpecularMap", "UseSpecularMap");
    TryApplyTexture(aiTextureType_METALNESS, "MetallicMap", "UseMetallicMap");
    TryApplyTexture(aiTextureType_AMBIENT_OCCLUSION, "AOMap", "UseAOMap");
    TryApplyTexture(aiTextureType_EMISSIVE, "EmissiveMap", "UseEmissiveMap");

    // Scalar fallbacks
    float Metallic = 0.0f;
    if (AssimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, Metallic) == AI_SUCCESS)
        MatInstance->SetScalarParameterValue("Metallic", Metallic);

    float Roughness = 0.5f;
    if (AssimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, Roughness) == AI_SUCCESS)
        MatInstance->SetScalarParameterValue("Roughness", Roughness);

    return MatInstance;
}

UTexture2D* AMeshLoader::CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName, aiTextureType Type)

{
    if (!EmbeddedTex)
    {
        UE_LOG(LogTemp, Warning, TEXT("Embedded texture is null: %s"), *DebugName);
        return nullptr;
    }

    if (EmbeddedTex->mHeight == 0) // Compressed texture (PNG, JPG, etc.)
    {
        const uint8* CompressedData = reinterpret_cast<const uint8*>(EmbeddedTex->pcData);
        int32 DataSize = EmbeddedTex->mWidth;

        if (!CompressedData || DataSize <= 0)
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Invalid compressed embedded texture data: %s"), *DebugName);
            return nullptr;
        }

        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
        EImageFormat Format = ImageWrapperModule.DetectImageFormat(CompressedData, DataSize);

        if (Format == EImageFormat::Invalid)
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Invalid image format in embedded texture: %s"), *DebugName);
            return nullptr;
        }

        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);

        if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(CompressedData, DataSize))
        {
            TArray64<uint8> RawData;
            if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
            {
                int32 Width = ImageWrapper->GetWidth();
                int32 Height = ImageWrapper->GetHeight();

                if (Width <= 0 || Height <= 0 || RawData.Num() != Width * Height * 4)
                {
                    UE_LOG(LogTemp, Error, TEXT("❌ Invalid image data size for embedded texture: %s"), *DebugName);
                    return nullptr;
                }

                UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
                if (!Texture)
                {
                    UE_LOG(LogTemp, Error, TEXT("❌ Failed to create texture object: %s"), *DebugName);
                    return nullptr;
                }

#if WITH_EDITORONLY_DATA
                Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
                Texture->NeverStream = true;
                Texture->LODGroup = TEXTUREGROUP_UI;
                if (Type == aiTextureType_DIFFUSE || Type == aiTextureType_BASE_COLOR)
                {
                    Texture->SRGB = true;  // Color data
                }
                else
                {
                    Texture->SRGB = false; // Linear data
                }


                // Safe locking and copying
                FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
                void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
                FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
                Mip.BulkData.Unlock();

                Texture->UpdateResource();

                // Don’t rename before UpdateResource
                Texture->SetFlags(RF_Transient);
                Texture->AddToRoot(); // Optional: Prevent GC
                UE_LOG(LogTemp, Display, TEXT("✅ Embedded texture created: %s (%dx%d)"), *DebugName, Width, Height);
                return Texture;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ Failed to decode raw data for: %s"), *DebugName);
            }
        }

        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to parse compressed embedded texture: %s"), *DebugName);
        }
    }
    else // Uncompressed raw RGBA texture (rare)
    {
        int32 Width = EmbeddedTex->mWidth;
        int32 Height = EmbeddedTex->mHeight;

        if (Width <= 0 || Height <= 0)
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Invalid raw texture dimensions for %s: %dx%d"), *DebugName, Width, Height);
            return nullptr;
        }

        UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height);
        if (!Texture)
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to create raw texture object: %s"), *DebugName);
            return nullptr;
        }

        void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(TextureData, EmbeddedTex->pcData, Width * Height * sizeof(FColor));
        Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

        Texture->UpdateResource();
        Texture->SetFlags(RF_Transient);
        Texture->Rename(*DebugName);

        return Texture;
    }

    UE_LOG(LogTemp, Warning, TEXT("⚠️ Unknown format or failed to create embedded texture: %s"), *DebugName);
    return nullptr;
}

UTexture2D* AMeshLoader::LoadTextureFromDisk(const FString& FbxFilePath)
{
    if (!FPaths::FileExists(FbxFilePath)) return nullptr;

    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FbxFilePath)) return nullptr;

    IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    EImageFormat Format = Module.DetectImageFormat(FileData.GetData(), FileData.Num());
    TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(Format);

    if (!Wrapper.IsValid() || !Wrapper->SetCompressed(FileData.GetData(), FileData.Num())) return nullptr;

    TArray64<uint8> RawData;
    if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, RawData)) return nullptr;

    int32 Width = Wrapper->GetWidth();
    int32 Height = Wrapper->GetHeight();

    UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height);
    if (!Texture) return nullptr;

    void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
    Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

    Texture->UpdateResource();
    Texture->SetFlags(RF_Transient);
    return Texture;
}

bool AMeshLoader::IsVectorFinite(const FVector& Vec)
{
    return FMath::IsFinite(Vec.X) && FMath::IsFinite(Vec.Y) && FMath::IsFinite(Vec.Z);
}

bool AMeshLoader::IsTransformValid(const FTransform& Transform)
{
    return IsVectorFinite(Transform.GetLocation()) && IsVectorFinite(Transform.GetScale3D());
}
