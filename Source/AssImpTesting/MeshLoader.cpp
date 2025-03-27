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
        UE_LOG(LogTemp, Error, TEXT("❌ FBX file not found: %s"), *FilePath);
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
        aiProcess_OptimizeMeshes |
        aiProcess_PreTransformVertices);

    if (!Scene || !Scene->mRootNode)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to load FBX: %s"), *FilePath);
        return;
    }

    ProcessNode(Scene->mRootNode, Scene, FTransform::Identity);
}

void AMeshLoader::ProcessNode(aiNode* Node, const aiScene* Scene, const FTransform& ParentTransform)
{
    FTransform NodeTransform = ConvertAssimpMatrix(Node->mTransformation) * ParentTransform;

    for (unsigned int i = 0; i < Node->mNumMeshes; ++i)
    {
        aiMesh* Mesh = Scene->mMeshes[Node->mMeshes[i]];
        ProcessMesh(Mesh, Scene, NodeTransform);
    }

    for (unsigned int i = 0; i < Node->mNumChildren; ++i)
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

    for (unsigned int i = 0; i < Mesh->mNumVertices; ++i)
    {
        FVector Position(Mesh->mVertices[i].x, Mesh->mVertices[i].z, Mesh->mVertices[i].y);
        Vertices.Add(Position);

        FVector Normal(Mesh->mNormals[i].x, Mesh->mNormals[i].z, Mesh->mNormals[i].y);
        Normals.Add(Normal.GetSafeNormal());

        FVector2D UV(0, 0);
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

    for (unsigned int i = 0; i < Mesh->mNumFaces; ++i)
    {
        aiFace Face = Mesh->mFaces[i];
        if (Face.mNumIndices == 3)
        {
            Triangles.Add(Face.mIndices[0]);
            Triangles.Add(Face.mIndices[1]);
            Triangles.Add(Face.mIndices[2]);
        }
    }

    UMaterialInterface* Material = nullptr;
    if (Mesh->mMaterialIndex >= 0 && Scene->mMaterials[Mesh->mMaterialIndex])
    {
        Material = CreateMaterialFromAssimp(Scene->mMaterials[Mesh->mMaterialIndex], Scene);
    }

    if (!Material)
    {
        Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }

    CreateProceduralMesh(Vertices, Triangles, Normals, UVs, Tangents, Bitangents, Material, Transform);
}

void AMeshLoader::CreateProceduralMesh(
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    const TArray<FVector>& Normals,
    const TArray<FVector2D>& UVs,
    const TArray<FVector>& Tangents,
    const TArray<FVector>& Bitangents,
    UMaterialInterface* Material,
    const FTransform& Transform)
{
    AActor* NewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Transform);
    UProceduralMeshComponent* ProcMesh = NewObject<UProceduralMeshComponent>(NewActor);
    ProcMesh->RegisterComponent();
    NewActor->SetRootComponent(ProcMesh);

    TArray<FProcMeshTangent> ProcTangents;
    for (int32 i = 0; i < Tangents.Num(); ++i)
    {
        ProcTangents.Add(FProcMeshTangent(Tangents[i], false));
    }

    ProcMesh->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UVs,
        TArray<FLinearColor>(),
        ProcTangents,
        true);

    ProcMesh->SetMaterial(0, Material);
    UE_LOG(LogTemp, Display, TEXT("✅ Spawned Procedural Mesh at %s"), *Transform.GetLocation().ToString());
}

FTransform AMeshLoader::ConvertAssimpMatrix(const aiMatrix4x4& M)
{
    FMatrix Matrix(
        FPlane(M.a1, M.a2, M.a3, M.a4),
        FPlane(M.b1, M.b2, M.b3, M.b4),
        FPlane(M.c1, M.c2, M.c3, M.c4),
        FPlane(M.d1, M.d2, M.d3, M.d4)
    );

    FVector Position = FVector(Matrix.M[3][0], Matrix.M[3][1], Matrix.M[3][2]);
    FVector Scale = Matrix.GetScaleVector();
    FQuat Rotation = FQuat(Matrix);
    return FTransform(Rotation, Position, Scale);
}

FString AMeshLoader::ResolveTexturePath(const FString& TexturePath)
{
    if (TexturePath.StartsWith("*"))
        return FString(); // Embedded handled separately

    FString BaseDir = FPaths::GetPath(FBXFilePath);
    FString FullPath = FPaths::Combine(BaseDir, TexturePath);

    TArray<FString> Extensions = { "", ".png", ".jpg", ".tga", ".dds" };
    for (const FString& Ext : Extensions)
    {
        if (FPaths::FileExists(FullPath + Ext))
        {
            return FullPath + Ext;
        }
    }

    return FString();
}

UTexture2D* AMeshLoader::LoadTextureFromDisk(const FString& FilePath)
{
    if (!FPaths::FileExists(FilePath)) return nullptr;

    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath)) return nullptr;

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

UMaterialInstanceDynamic* AMeshLoader::CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene)
{
    if (!MasterMaterial)
    {
        static const FString Path = TEXT("/Game/Materials/M_BaseMaterial.M_BaseMaterial");
        MasterMaterial = LoadObject<UMaterial>(nullptr, *Path);
    }

    if (!MasterMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No master material"));
        return nullptr;
    }

    UMaterialInstanceDynamic* MatInstance = UMaterialInstanceDynamic::Create(MasterMaterial, GetTransientPackage());
    if (!MatInstance) return nullptr;

    const FString BaseDir = FPaths::GetPath(FBXFilePath);

    auto TryApplyTexture = [&](aiTextureType Type, const FName& Param, const FName& EnableParam)
        {
            aiString TexPath;
            if (AssimpMaterial->GetTexture(Type, 0, &TexPath) == AI_SUCCESS)
            {
                FString Path = UTF8_TO_TCHAR(TexPath.C_Str());
                FString FullPath = FPaths::Combine(BaseDir, Path);
                FPaths::NormalizeFilename(FullPath);

                UTexture2D* Texture = LoadTextureFromDisk(FullPath);
                if (Texture)
                {
                    MatInstance->SetTextureParameterValue(Param, Texture);
                    if (!EnableParam.IsNone())
                        MatInstance->SetScalarParameterValue(EnableParam, 1.0f);
                }
            }
        };

    TryApplyTexture(aiTextureType_DIFFUSE, "BaseColorTex", "UseBaseColorTex");
    TryApplyTexture(aiTextureType_NORMALS, "NormalMap", "UseNormalMap");
    TryApplyTexture(aiTextureType_METALNESS, "MetallicMap", "UseMetallicMap");
    TryApplyTexture(aiTextureType_AMBIENT_OCCLUSION, "AOMap", "UseAOMap");

    return MatInstance;
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
