﻿// Class Added by Ebaad, This class deals with Model Raw Data Extraction using Assimp , Creating Model From that Data and Spawning on Demand in Scene On Certain Location
#include "AssimpRuntime3DModelsImporter.h"
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

UAssimpRuntime3DModelsImporter::UAssimpRuntime3DModelsImporter() {
}

void UAssimpRuntime3DModelsImporter::LoadFBXModel(const FString& InFilePath)
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

    RootNode = FModelNodeData();
    ParseNode(Scene->mRootNode, Scene, RootNode, FilePath);
}

void UAssimpRuntime3DModelsImporter::ParseNode(aiNode* Node, const aiScene* Scene, FModelNodeData& OutNode, const FString& FbxFilePath) {
    OutNode.Name = UTF8_TO_TCHAR(Node->mName.C_Str());
    OutNode.Transform = ConvertAssimpMatrix(Node->mTransformation);
    if (FbxFilePath.EndsWith(".glb") || FbxFilePath.EndsWith(".gltf"))
    {
        OutNode.Transform.SetScale3D(FVector(100.0f)); // adjust accordingly
    }


    for (uint32 i = 0; i < Node->mNumMeshes; ++i) {
        aiMesh* Mesh = Scene->mMeshes[Node->mMeshes[i]];
        FModelMeshData MeshData;
        ExtractMesh(Mesh, Scene, MeshData, FbxFilePath);
        OutNode.MeshSections.Add(MoveTemp(MeshData));
    }

    for (uint32 i = 0; i < Node->mNumChildren; ++i) {
        FModelNodeData ChildNode;
        ParseNode(Node->mChildren[i], Scene, ChildNode, FbxFilePath);
        OutNode.Children.Add(MoveTemp(ChildNode));
    }
}

void UAssimpRuntime3DModelsImporter::ExtractMesh(aiMesh* Mesh, const aiScene* Scene, FModelMeshData& OutMesh, const FString& FbxFilePath) {
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

AActor* UAssimpRuntime3DModelsImporter::SpawnModel(UWorld* World, const FVector& SpawnLocation)
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

    for (FModelNodeData& Child : RootNode.Children)
    {
        SpawnNodeRecursive(Child, RootActor);
    }

    return RootActor;
}

void UAssimpRuntime3DModelsImporter::SpawnNodeRecursive(const FModelNodeData& Node, AActor* Parent)
{
    AActor* NodeActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
#if WITH_EDITOR
    NodeActor->SetActorLabel(Node.Name);
#endif

    // ✅ Store reference in map
    SpawnedNodeActors.Add(Node.Name, NodeActor);

    // ✅ Attach and transform setup
    USceneComponent* RootComp = NewObject<USceneComponent>(NodeActor);
    RootComp->RegisterComponent();
    NodeActor->SetRootComponent(RootComp);
    NodeActor->AttachToActor(Parent, FAttachmentTransformRules::KeepRelativeTransform);
    NodeActor->SetActorRelativeTransform(Node.Transform);

    // ✅ Create mesh sections
    for (const FModelMeshData& Section : Node.MeshSections)
    {
        UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(NodeActor);
        Mesh->RegisterComponent();
        Mesh->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
        NodeActor->AddInstanceComponent(Mesh); // ✅ ADD THIS LINE


        TArray<FProcMeshTangent> Tangents;
        for (const FVector& Tangent : Section.Tangents)
        {
            Tangents.Add(FProcMeshTangent(Tangent, false));
        }

        Mesh->CreateMeshSection_LinearColor(0, Section.Vertices, Section.Triangles, Section.Normals, Section.UVs, {}, Tangents, true);
        Mesh->SetMaterial(0, Section.Material ? Section.Material : LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
    }

    // ✅ Recursively spawn children
    for (const FModelNodeData& Child : Node.Children)
    {
        SpawnNodeRecursive(Child, NodeActor);
    }
}

FTransform UAssimpRuntime3DModelsImporter::ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix)
{
    aiVector3D Scaling, Position;
    aiQuaternion Rotation;

    AssimpMatrix.Decompose(Scaling, Rotation, Position);

    FVector UE_Position = FVector(Position.x, Position.y, Position.z);
    FQuat UE_Rotation = FQuat(Rotation.x, Rotation.y, Rotation.z, Rotation.w);
    FVector UE_Scale = FVector(Scaling.x, Scaling.y, Scaling.z);

    // Optional Fix: Flip Y/Z for coordinate system compatibility
    // (Unreal is Z-up, Y-forward. FBX is usually Z-up, GLTF is Y-up)
    UE_Position = FVector(UE_Position.X, UE_Position.Z, UE_Position.Y); // Swap Y & Z
    UE_Rotation = FQuat(UE_Rotation.X, UE_Rotation.Z, UE_Rotation.Y, -UE_Rotation.W); // Adjust axes
    UE_Scale = FVector(UE_Scale.X, UE_Scale.Z, UE_Scale.Y); // Swap Y & Z for scale too

    return FTransform(UE_Rotation, UE_Position, UE_Scale);
}

void UAssimpRuntime3DModelsImporter::LoadMasterMaterial()
{
    MasterMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_BaseMaterial.M_BaseMaterial"));

    if (!MasterMaterial)
    {
        MasterMaterial = LoadObject<UMaterial>(nullptr, TEXT("/RuntimeModelsImporter/Materials/M_BaseMaterial.M_BaseMaterial"));
    }

    if (!MasterMaterial)
    {
        MasterMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No custom M_BaseMaterial found. Using Engine fallback."));
    }
}

UMaterialInstanceDynamic* UAssimpRuntime3DModelsImporter::CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene, const FString& FbxFilePath)
{
    if (!AssimpMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Null AssimpMaterial provided."));
        return nullptr;
    }

    // ✅ Reuse material if already processed
    if (MaterialCache.Contains(AssimpMaterial))
        return MaterialCache[AssimpMaterial];

    // ✅ Load base material
    if (!MasterMaterial)
        LoadMasterMaterial();
    if (!MasterMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No valid master material loaded."));
        return nullptr;
    }

    // ✅ Create dynamic material
    UMaterialInstanceDynamic* MatInstance = UMaterialInstanceDynamic::Create(MasterMaterial, GetTransientPackage());
    if (!MatInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create dynamic material instance."));
        return nullptr;
    }

    const FString BaseDir = FPaths::GetPath(FbxFilePath);

    // ✅ Store for reuse
    LoadedMaterials.Add(MatInstance);
    MaterialCache.Add(AssimpMaterial, MatInstance);

    // 🎨 Fallback base color
    aiColor3D DiffuseColor(1.0f, 1.0f, 1.0f);
    if (AssimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, DiffuseColor) == AI_SUCCESS)
    {
        FLinearColor Color(DiffuseColor.r, DiffuseColor.g, DiffuseColor.b);
        MatInstance->SetVectorParameterValue("BaseColor", Color);
        UE_LOG(LogTemp, Warning, TEXT("🎨 Diffuse Fallback Color: R=%.2f G=%.2f B=%.2f"), Color.R, Color.G, Color.B);
    }

    // 📦 Texture mapping function with logging
    auto TryApplyTexture = [&](aiTextureType Type, const FName& ParamName, const FName& EnableParamName)
        {
            aiString TexPath;
            if (AssimpMaterial->GetTexture(Type, 0, &TexPath) == AI_SUCCESS)
            {
                FString Path = UTF8_TO_TCHAR(TexPath.C_Str());
                UTexture2D* Texture = nullptr;

                // Handle embedded texture
                if (Path.StartsWith(TEXT("*")))
                {
                    const aiTexture* Embedded = Scene->GetEmbeddedTexture(TexPath.C_Str());
                    Texture = CreateTextureFromEmbedded(Embedded, Path, Type);
                    UE_LOG(LogTemp, Log, TEXT("📦 Using embedded texture for %s: %s"), *ParamName.ToString(), *Path);
                }
                else
                {
                    // Handle external texture
                    FString FileNameOnly = FPaths::GetCleanFilename(Path);
                    FString FoundTexturePath;

                    IFileManager& FileManager = IFileManager::Get();
                    TArray<FString> FoundFiles;
                    FileManager.FindFilesRecursive(FoundFiles, *BaseDir, *FileNameOnly, true, false);

                    if (FoundFiles.Num() > 0)
                    {
                        FoundTexturePath = FoundFiles[0];
                    }
                    else
                    {
                        FoundTexturePath = FPaths::Combine(BaseDir, Path);
                        FPaths::NormalizeFilename(FoundTexturePath);
                    }

                    if (!FoundTexturePath.IsEmpty() && FPaths::FileExists(FoundTexturePath))
                    {
                        Texture = LoadTextureFromDisk(FoundTexturePath);
                        UE_LOG(LogTemp, Log, TEXT("🖼️ Found external texture for %s: %s"), *ParamName.ToString(), *FoundTexturePath);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("❌ Texture file not found for %s → %s"), *ParamName.ToString(), *Path);
                    }
                }

                if (Texture)
                {
                    MatInstance->SetTextureParameterValue(ParamName, Texture);
                    if (!EnableParamName.IsNone())
                        MatInstance->SetScalarParameterValue(EnableParamName, 1.0f);
                    UE_LOG(LogTemp, Display, TEXT("✅ Texture applied to %s | Enable: %s = 1"), *ParamName.ToString(), *EnableParamName.ToString());
                }
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("⚠️ No texture assigned for %s (aiTextureType %d)"), *ParamName.ToString(), (int32)Type);
            }
        };

    // 🔍 Texture channels with logging
    TryApplyTexture(aiTextureType_DIFFUSE, "BaseColorTex", "UseBaseColorTex");
    TryApplyTexture(aiTextureType_BASE_COLOR, "BaseColorTex", "UseBaseColorTex");
    TryApplyTexture(aiTextureType_NORMALS, "NormalMap", "UseNormalMap");
    TryApplyTexture(aiTextureType_HEIGHT, "NormalMap", "UseNormalMap");
    TryApplyTexture(aiTextureType_SPECULAR, "SpecularMap", "UseSpecularMap");
    TryApplyTexture(aiTextureType_METALNESS, "MetallicMap", "UseMetallicMap");
    TryApplyTexture(aiTextureType_AMBIENT_OCCLUSION, "AOMap", "UseAOMap");
    TryApplyTexture(aiTextureType_EMISSIVE, "EmissiveMap", "UseEmissiveMap");
    TryApplyTexture(aiTextureType_DIFFUSE_ROUGHNESS, "RoughnessMap", "UseRoughnessMap");
    TryApplyTexture(aiTextureType_OPACITY, "OpacityMap", "UseOpacityMap");

    // ⚙️ Scalar fallback logging for all items in material except BaseTex and Normal Text   
    float Metallic = 0.0f;
    if (AssimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, Metallic) == AI_SUCCESS)
    {
        MatInstance->SetScalarParameterValue("Metallic", Metallic);
        UE_LOG(LogTemp, Log, TEXT("⚙️ Metallic fallback scalar used: %.2f"), Metallic);
    }

    aiColor3D SpecularColor;
    if (AssimpMaterial->Get(AI_MATKEY_COLOR_SPECULAR, SpecularColor) == AI_SUCCESS)
    {
        FLinearColor SpecColor(SpecularColor.r, SpecularColor.g, SpecularColor.b);
        MatInstance->SetVectorParameterValue("SpecularColor", SpecColor);
        UE_LOG(LogTemp, Log, TEXT("🎯 Specular Color fallback: R=%.2f G=%.2f B=%.2f"), SpecColor.R, SpecColor.G, SpecColor.B);
    }

    float Roughness = 0.5f;
    if (AssimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, Roughness) == AI_SUCCESS)
    {
        MatInstance->SetScalarParameterValue("Roughness", Roughness);
        UE_LOG(LogTemp, Log, TEXT("⚙️ Roughness fallback scalar used: %.2f"), Roughness);
    }

    aiColor3D EmissiveColor;
    if (AssimpMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, EmissiveColor) == AI_SUCCESS)
    {
        FLinearColor Emissive(EmissiveColor.r, EmissiveColor.g, EmissiveColor.b);
        MatInstance->SetVectorParameterValue("EmissiveColor", Emissive);
        UE_LOG(LogTemp, Log, TEXT("🌟 Emissive fallback: R=%.2f G=%.2f B=%.2f"), Emissive.R, Emissive.G, Emissive.B);
    }

    float Opacity = 1.0f;
    if (AssimpMaterial->Get(AI_MATKEY_OPACITY, Opacity) == AI_SUCCESS)
    {
        MatInstance->SetScalarParameterValue("Opacity", Opacity);
        UE_LOG(LogTemp, Log, TEXT("🔍 Opacity fallback scalar: %.2f"), Opacity);
    }

    float AO = 1.0f;
    if (AssimpMaterial->Get(AI_MATKEY_OPACITY, AO) == AI_SUCCESS)
    {
        MatInstance->SetScalarParameterValue("AO", AO);
        UE_LOG(LogTemp, Log, TEXT("🔍 AO fallback scalar: %.2f"), AO);
    }

    return MatInstance;
}



UTexture2D* UAssimpRuntime3DModelsImporter::CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName, aiTextureType Type)

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

UTexture2D* UAssimpRuntime3DModelsImporter::LoadTextureFromDisk(const FString& FbxFilePath)
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

bool UAssimpRuntime3DModelsImporter::IsVectorFinite(const FVector& Vec)
{
    return FMath::IsFinite(Vec.X) && FMath::IsFinite(Vec.Y) && FMath::IsFinite(Vec.Z);
}

bool UAssimpRuntime3DModelsImporter::IsTransformValid(const FTransform& Transform)
{
    return IsVectorFinite(Transform.GetLocation()) && IsVectorFinite(Transform.GetScale3D());
}

void UAssimpRuntime3DModelsImporter::LoadAssimpDLLIfNeeded()
{
    static bool bLoaded = false;
    if (bLoaded) return;

    FString PluginDir = FPaths::ProjectPluginsDir();  // Handles both Editor & Packaged
    FString DllPath = FPaths::Combine(PluginDir, TEXT("RuntimeModelsImporter/Binaries/Win64/assimp-vc143-mt.dll"));

    if (FPaths::FileExists(DllPath))
    {
        void* Handle = FPlatformProcess::GetDllHandle(*DllPath);
        if (Handle)
        {
            bLoaded = true;
            UE_LOG(LogTemp, Display, TEXT("✅ Assimp DLL loaded successfully from: %s"), *DllPath);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to load Assimp DLL from: %s"), *DllPath);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Assimp DLL not found at: %s"), *DllPath);
    }
}

AActor* UAssimpRuntime3DModelsImporter::GetNodeActorByName(const FString& NodeName) const
{
    if (SpawnedNodeActors.Contains(NodeName))
    {
        return SpawnedNodeActors[NodeName];
    }
    return nullptr;
}







