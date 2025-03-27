#include "MeshLoader.h"
#include "StaticMeshAttributes.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Engine/StaticMeshActor.h"
#include "MeshLoader.h"
#include "StaticMeshAttributes.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Engine/StaticMeshActor.h"
#include "PhysicsEngine/BodySetup.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"

AMeshLoader::AMeshLoader()
{
    PrimaryActorTick.bCanEverTick = false;
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
    SetRootComponent(StaticMeshComponent);
}

void AMeshLoader::BeginPlay()
{
    Super::BeginPlay();
    LoadFBXModel(FBXFilePath);
}

void AMeshLoader::LoadFBXModel(const FString& FilePath)
{
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("FBX file not found: %s"), *FilePath);
        return;
    }

    Assimp::Importer Importer;
    const aiScene* Scene = Importer.ReadFile(TCHAR_TO_UTF8(*FilePath),
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes | aiProcess_PreTransformVertices);

    if (!Scene || !Scene->mRootNode)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load FBX: %s"), *FilePath);
        return;
    }

    ProcessNode(Scene->mRootNode, Scene, FTransform::Identity);
}

void AMeshLoader::ProcessNode(aiNode* Node, const aiScene* Scene, const FTransform& ParentTransform)
{
    FTransform NodeTransform = ConvertAssimpMatrix(Node->mTransformation) * ParentTransform;

    for (unsigned int i = 0; i < Node->mNumMeshes; i++)
    {
        aiMesh* Mesh = Scene->mMeshes[Node->mMeshes[i]];
        ProcessMesh(Mesh, Scene, NodeTransform);
    }

    for (unsigned int i = 0; i < Node->mNumChildren; i++)
    {
        ProcessNode(Node->mChildren[i], Scene, NodeTransform);
    }
}

void AMeshLoader::ProcessMesh(aiMesh* Mesh, const aiScene* Scene, const FTransform& Transform)
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FVector> Tangents;
    TArray<FVector> Bitangents;

    for (unsigned int i = 0; i < Mesh->mNumVertices; i++)
    {
        FVector Position(Mesh->mVertices[i].x, Mesh->mVertices[i].z, Mesh->mVertices[i].y);
        Vertices.Add(Position);

        FVector Normal(Mesh->mNormals[i].x, Mesh->mNormals[i].z, Mesh->mNormals[i].y);
        Normals.Add(Normal.GetSafeNormal());

        FVector2D UV(0.0f, 0.0f);
        if (Mesh->HasTextureCoords(0))
        {
            UV = FVector2D(Mesh->mTextureCoords[0][i].x, Mesh->mTextureCoords[0][i].y);
        }
        UVs.Add(UV);

        if (Mesh->HasTangentsAndBitangents())
        {
            FVector Tangent(Mesh->mTangents[i].x, -Mesh->mTangents[i].z, Mesh->mTangents[i].y);
            FVector Bitangent(Mesh->mBitangents[i].x, -Mesh->mBitangents[i].z, Mesh->mBitangents[i].y);
            Tangents.Add(Tangent);
            Bitangents.Add(Bitangent);
        }
    }

    for (unsigned int i = 0; i < Mesh->mNumFaces; i++)
    {
        aiFace Face = Mesh->mFaces[i];
        if (Face.mNumIndices == 3)
        {
            Triangles.Add(Face.mIndices[0]);
            Triangles.Add(Face.mIndices[1]);
            Triangles.Add(Face.mIndices[2]);
        }
    }

    UStaticMesh* StaticMesh = CreateStaticMesh(Vertices, Triangles, Normals, UVs, Tangents, Bitangents);
    if (!StaticMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create StaticMesh"));
        return;
    }

    LoadedMeshes.Add(StaticMesh);

    UMaterialInterface* Material = nullptr;
    if (Mesh->mMaterialIndex >= 0 && Scene->mMaterials[Mesh->mMaterialIndex])
    {
        Material = CreateMaterialFromAssimp(Scene->mMaterials[Mesh->mMaterialIndex], Scene);
    }

    if (!Material)
    {
        Material = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }

    AStaticMeshActor* MeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
    MeshActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
    MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    MeshActor->GetStaticMeshComponent()->SetMaterial(0, Material);
    MeshActor->SetActorScale3D(Transform.GetScale3D());

    const FBoxSphereBounds Bounds = StaticMesh->GetBounds();
    UE_LOG(LogTemp, Display, TEXT("✅ Spawned actor at Location: %s | Scale: %s"),
        *Transform.GetLocation().ToString(),
        *Transform.GetScale3D().ToString());

    UE_LOG(LogTemp, Display, TEXT("📦 Mesh bounds: Origin=%s | Extent=%s"),
        *Bounds.Origin.ToString(),
        *Bounds.BoxExtent.ToString());
}


UStaticMesh* AMeshLoader::CreateStaticMesh(
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    const TArray<FVector>& Normals,
    const TArray<FVector2D>& UVs,
    const TArray<FVector>& Tangents,
    const TArray<FVector>& Bitangents)
{
    FMeshDescription MeshDescription;
    FStaticMeshAttributes Attributes(MeshDescription);
    Attributes.Register();

    TMap<int32, FVertexID> IndexToVertex;

    for (int32 i = 0; i < Vertices.Num(); ++i)
    {
        FVertexID VertexID = MeshDescription.CreateVertex();
        Attributes.GetVertexPositions()[VertexID] = FVector3f(Vertices[i]);
        IndexToVertex.Add(i, VertexID);
    }

    FPolygonGroupID PolygonGroupID = MeshDescription.CreatePolygonGroup();

    for (int32 i = 0; i < Triangles.Num(); i += 3)
    {
        TArray<FVertexInstanceID> VertexInstanceIDs;
        for (int32 j = 0; j < 3; ++j)
        {
            int32 Index = Triangles[i + j];
            FVertexInstanceID InstanceID = MeshDescription.CreateVertexInstance(IndexToVertex[Index]);

            if (Normals.IsValidIndex(Index))
                Attributes.GetVertexInstanceNormals()[InstanceID] = FVector3f(Normals[Index]);

            if (UVs.IsValidIndex(Index))
                Attributes.GetVertexInstanceUVs()[InstanceID] = FVector2f(UVs[Index]);

            if (Tangents.IsValidIndex(Index))
                Attributes.GetVertexInstanceTangents()[InstanceID] = FVector3f(Tangents[Index]);

            if (Bitangents.IsValidIndex(Index) && Normals.IsValidIndex(Index))
            {
                float Sign = FVector::DotProduct(
                    FVector::CrossProduct(Normals[Index], Tangents[Index]),
                    Bitangents[Index]) < 0 ? -1.0f : 1.0f;
                Attributes.GetVertexInstanceBinormalSigns()[InstanceID] = Sign;
            }

            VertexInstanceIDs.Add(InstanceID);
        }

        MeshDescription.CreatePolygon(PolygonGroupID, VertexInstanceIDs);
    }

    // Now use BuildFromMeshDescriptions instead of SetMeshDescription
    TArray<const FMeshDescription*> MeshDescArray;
    MeshDescArray.Add(&MeshDescription);

    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
    StaticMesh->BuildFromMeshDescriptions(MeshDescArray);

    return StaticMesh;
}




FTransform AMeshLoader::ConvertAssimpMatrix(const aiMatrix4x4& AssimpMatrix)
{
    FMatrix UnrealMatrix(
        FPlane(AssimpMatrix.a1, AssimpMatrix.a2, AssimpMatrix.a3, AssimpMatrix.a4),
        FPlane(AssimpMatrix.b1, AssimpMatrix.b2, AssimpMatrix.b3, AssimpMatrix.b4),
        FPlane(AssimpMatrix.c1, AssimpMatrix.c2, AssimpMatrix.c3, AssimpMatrix.c4),
        FPlane(AssimpMatrix.d1, AssimpMatrix.d2, AssimpMatrix.d3, AssimpMatrix.d4)
    );

    FVector Position = FVector(UnrealMatrix.M[3][0], UnrealMatrix.M[3][1], UnrealMatrix.M[3][2]);
    FVector Scale = FVector(UnrealMatrix.GetScaleVector());
    FQuat Rotation = FQuat(UnrealMatrix);

    return FTransform(Rotation, Position, Scale);
}

FString AMeshLoader::ResolveTexturePath(const FString& TexturePath)
{
    // Handle embedded textures
    if (TexturePath.StartsWith("*"))
    {
        UE_LOG(LogTemp, Warning, TEXT("Embedded textures not yet implemented"));
        return FString();
    }

    // Convert relative paths
    FString BaseDir = FPaths::GetPath(FBXFilePath);
    FString FullPath = FPaths::Combine(BaseDir, TexturePath);

    // Check common extensions
    TArray<FString> Extensions = { "", ".png", ".jpg", ".tga", ".dds" };
    for (const FString& Ext : Extensions)
    {
        if (FPaths::FileExists(FullPath + Ext))
        {
            return FullPath + Ext;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Texture not found: %s"), *TexturePath);
    return FString();
}

UTexture2D* AMeshLoader::LoadTextureFromFile(const FString& FullPath)
{
    if (FullPath.IsEmpty()) return nullptr;

    UTexture2D* Texture = Cast<UTexture2D>(
        StaticLoadObject(UTexture2D::StaticClass(), nullptr, *FullPath));

    if (!Texture)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load texture: %s"), *FullPath);
    }
    return Texture;
}

void AMeshLoader::LoadMasterMaterial()
{
    static const FString MaterialPath = TEXT("/Game/Materials/M_BaseMaterial.M_BaseMaterial");
    MasterMaterial = LoadObject<UMaterial>(nullptr, *MaterialPath);

    if (!MasterMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load master material at path: %s"), *MaterialPath);
        MasterMaterial = LoadObject<UMaterial>(nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }
}

UTexture2D* AMeshLoader::LoadTextureFromDisk(const FString& FilePath)
{
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Texture file not found: %s"), *FilePath);
        return nullptr;
    }

    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load file data: %s"), *FilePath);
        return nullptr;
    }

    // Load image wrapper
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    EImageFormat Format = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
    if (Format == EImageFormat::Invalid)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unrecognized image format for file: %s"), *FilePath);
        return nullptr;
    }

    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);
    if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to parse image data: %s"), *FilePath);
        return nullptr;
    }

    TArray64<uint8> RawData;
    if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to extract raw pixel data: %s"), *FilePath);
        return nullptr;
    }

    int32 Width = ImageWrapper->GetWidth();
    int32 Height = ImageWrapper->GetHeight();

    UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height);
    if (!Texture)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create transient texture: %s"), *FilePath);
        return nullptr;
    }

    void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
    Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

    Texture->UpdateResource();
    Texture->SetFlags(RF_Transient);
    Texture->Rename(*FPaths::GetBaseFilename(FilePath));

    return Texture;
}

UMaterialInstanceDynamic* AMeshLoader::CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene)
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
    const FString BaseDir = FPaths::GetPath(FBXFilePath);

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
                    Texture = CreateTextureFromEmbedded(Embedded, Path);
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

UTexture2D* AMeshLoader::CreateTextureFromEmbedded(const aiTexture* EmbeddedTex, const FString& DebugName)
{
    if (!EmbeddedTex)
    {
        UE_LOG(LogTemp, Warning, TEXT("Embedded texture is null: %s"), *DebugName);
        return nullptr;
    }

    if (EmbeddedTex->mHeight == 0) // Compressed (e.g., PNG, JPG)
    {
        const uint8* CompressedData = reinterpret_cast<const uint8*>(EmbeddedTex->pcData);
        int32 DataSize = EmbeddedTex->mWidth;

        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
        EImageFormat Format = ImageWrapperModule.DetectImageFormat(CompressedData, DataSize);
        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);

        if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(CompressedData, DataSize))
        {
            TArray64<uint8> RawData;
            if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
            {
                int32 Width = ImageWrapper->GetWidth();
                int32 Height = ImageWrapper->GetHeight();

                UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height);
                if (!Texture) return nullptr;

                void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
                FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
                Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

                Texture->UpdateResource();
                Texture->SetFlags(RF_Transient);
                Texture->Rename(*DebugName);
                return Texture;
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("Failed to decode embedded compressed texture: %s"), *DebugName);
    }
    else // Raw RGBA texture
    {
        int32 Width = EmbeddedTex->mWidth;
        int32 Height = EmbeddedTex->mHeight;

        UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height);
        if (!Texture)
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to create raw embedded texture: %s"), *DebugName);
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

    return nullptr;
}

