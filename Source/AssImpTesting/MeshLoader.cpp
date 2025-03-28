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
}

void AMeshLoader::BeginPlay()
{
    Super::BeginPlay();
    LoadFBXFilesFromFolder(TEXT("C:/Users/ebaad.hanif/Desktop/FBX Models"));
}

void AMeshLoader::LoadFBXFilesFromFolder(const FString& FbxFolderPath)
{
    FString SearchPath = FbxFolderPath / TEXT("*.fbx");
    TArray<FString> FoundFiles;

    IFileManager::Get().FindFiles(FoundFiles, *SearchPath, true, false);

    for (const FString& FileName : FoundFiles)
    {
        FString FullPath = FPaths::Combine(FbxFolderPath, FileName);
        LoadFBXModel(FullPath);
    }
}

void AMeshLoader::LoadFBXModel(const FString& FilePath)
{
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("File does not exist: %s"), *FilePath);
        return;
    }

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
        UE_LOG(LogTemp, Error, TEXT("Assimp failed to load FBX: %s"), *FilePath);
        return;
    }

    RootFBXActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
    RootFBXActor->SetActorLabel(FPaths::GetBaseFilename(FilePath));

    USceneComponent* SceneRoot = NewObject<USceneComponent>(RootFBXActor);
    SceneRoot->RegisterComponent();
    RootFBXActor->SetRootComponent(SceneRoot);

    ProcessNode(Scene->mRootNode, Scene, SceneRoot, FilePath);
}

void AMeshLoader::ProcessNode(aiNode* Node, const aiScene* Scene, USceneComponent* MyParentComponent, const FString& FilePath)
{
    USceneComponent* ThisComponent = CreateNodeComponent(Node, Node->mTransformation, MyParentComponent);

    for (uint32 i = 0; i < Node->mNumMeshes; ++i)
    {
        aiMesh* Mesh = Scene->mMeshes[Node->mMeshes[i]];
        ProcessMesh(Mesh, Scene, ThisComponent, FilePath, FTransform::Identity);
    }

    for (uint32 i = 0; i < Node->mNumChildren; ++i)
    {
        ProcessNode(Node->mChildren[i], Scene, ThisComponent, FilePath);
    }
}

void AMeshLoader::ProcessMesh(aiMesh* Mesh, const aiScene* Scene, USceneComponent* MyParentComponent, const FString& FilePath, const FTransform& LocalTransform)
{
    FString MeshName = UTF8_TO_TCHAR(Mesh->mName.C_Str());
    UE_LOG(LogTemp, Display, TEXT("🛠️ Processing Mesh: %s"), *MeshName);

    TArray<FVector> Vertices, Normals, Tangents, Bitangents;
    TArray<int32> Triangles;
    TArray<FVector2D> UVs;

    for (uint32 i = 0; i < Mesh->mNumVertices; ++i)
    {
        Vertices.Add(FVector(Mesh->mVertices[i].x, Mesh->mVertices[i].z, Mesh->mVertices[i].y));
        Normals.Add(FVector(Mesh->mNormals[i].x, Mesh->mNormals[i].z, Mesh->mNormals[i].y).GetSafeNormal());

        UVs.Add(Mesh->HasTextureCoords(0)
            ? FVector2D(Mesh->mTextureCoords[0][i].x, Mesh->mTextureCoords[0][i].y)
            : FVector2D::ZeroVector);

        if (Mesh->HasTangentsAndBitangents())
        {
            Tangents.Add(FVector(Mesh->mTangents[i].x, -Mesh->mTangents[i].z, Mesh->mTangents[i].y));
            Bitangents.Add(FVector(Mesh->mBitangents[i].x, -Mesh->mBitangents[i].z, Mesh->mBitangents[i].y));
        }
    }

    for (uint32 i = 0; i < Mesh->mNumFaces; ++i)
    {
        aiFace& Face = Mesh->mFaces[i];
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
        Material = CreateMaterialFromAssimp(Scene->mMaterials[Mesh->mMaterialIndex], Scene, FilePath);
    }

    if (!Material)
    {
        Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }

    CreateProceduralMesh(Vertices, Triangles, Normals, UVs, Tangents, Bitangents, Material, MyParentComponent, LocalTransform, MeshName);
}

void AMeshLoader::CreateProceduralMesh(
    const TArray<FVector>& Vertices,
    const TArray<int32>& Triangles,
    const TArray<FVector>& Normals,
    const TArray<FVector2D>& UVs,
    const TArray<FVector>& Tangents,
    const TArray<FVector>& Bitangents,
    UMaterialInterface* Material,
    USceneComponent* MyParentComponent,
    const FTransform& LocalTransform,
    const FString& MeshName)
{
    if (Vertices.Num() == 0 || Triangles.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No geometry for mesh: %s"), *MeshName);
        return;
    }

    FName ValidName = MeshName.IsEmpty()
        ? MakeUniqueObjectName(GetTransientPackage(), UProceduralMeshComponent::StaticClass(), TEXT("Mesh"))
        : *MeshName;

    UProceduralMeshComponent* ProcMesh = NewObject<UProceduralMeshComponent>(
        GetTransientPackage(),
        UProceduralMeshComponent::StaticClass(),
        ValidName,
        RF_Transient
    );

    if (!ProcMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create ProceduralMeshComponent: %s"), *MeshName);
        return;
    }

    ProcMesh->RegisterComponentWithWorld(GetWorld());
    ProcMesh->AttachToComponent(MyParentComponent, FAttachmentTransformRules::KeepRelativeTransform);
    ProcMesh->SetRelativeTransform(LocalTransform);
    RootFBXActor->AddInstanceComponent(ProcMesh);

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
        true
    );

    ProcMesh->SetMaterial(0, Material);

    UE_LOG(LogTemp, Display, TEXT("✅ Created mesh: %s | Verts: %d | Tris: %d"), *MeshName, Vertices.Num(), Triangles.Num());
}

USceneComponent* AMeshLoader::CreateNodeComponent(aiNode* Node, const aiMatrix4x4& TransformMatrix, USceneComponent* MyParentComponent)
{
    FString NodeName = UTF8_TO_TCHAR(Node->mName.C_Str());
    FName ValidName = NodeName.IsEmpty() ? MakeUniqueObjectName(RootFBXActor, USceneComponent::StaticClass(), TEXT("Node")) : *NodeName;

    USceneComponent* NodeComp = NewObject<USceneComponent>(RootFBXActor, USceneComponent::StaticClass(), ValidName, RF_Transient);
    if (!NodeComp)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create node: %s"), *NodeName);
        return nullptr;
    }

    RootFBXActor->AddInstanceComponent(NodeComp);
    NodeComp->AttachToComponent(MyParentComponent, FAttachmentTransformRules::KeepRelativeTransform);
    NodeComp->RegisterComponent();

    FTransform LocalTransform = ConvertAssimpMatrix(TransformMatrix);
    if (!IsTransformValid(LocalTransform))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Node has invalid transform: %s, using Identity"), *NodeName);
        LocalTransform = FTransform::Identity;
    }

    NodeComp->SetRelativeTransform(LocalTransform);
    return NodeComp;
}

FTransform AMeshLoader::ConvertAssimpMatrix(const aiMatrix4x4& M)
{
    FMatrix Matrix(
        FPlane(M.a1, M.a2, M.a3, M.a4),
        FPlane(M.b1, M.b2, M.b3, M.b4),
        FPlane(M.c1, M.c2, M.c3, M.c4),
        FPlane(M.d1, M.d2, M.d3, M.d4)
    );

    FVector Location = FVector(Matrix.M[3][0], Matrix.M[3][1], Matrix.M[3][2]);
    FVector Scale = Matrix.GetScaleVector();
    FRotator Rotator = Matrix.Rotator();
    FQuat Rotation = FQuat(Rotator);
    FTransform Result(Rotation, Location, Scale);

    return Result;
}

bool IsVectorFinite(const FVector& Vec)
{
    return FMath::IsFinite(Vec.X) && FMath::IsFinite(Vec.Y) && FMath::IsFinite(Vec.Z);
}

bool IsTransformValid(const FTransform& Transform)
{
    return IsVectorFinite(Transform.GetLocation()) && IsVectorFinite(Transform.GetScale3D());
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

UMaterialInstanceDynamic* AMeshLoader::CreateMaterialFromAssimp(aiMaterial* AssimpMaterial, const aiScene* Scene, const FString& FilePath)
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
    const FString BaseDir = FPaths::GetPath(FilePath);

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

bool AMeshLoader::IsVectorFinite(const FVector& Vec)
{
    return FMath::IsFinite(Vec.X) && FMath::IsFinite(Vec.Y) && FMath::IsFinite(Vec.Z);
}


bool AMeshLoader::IsTransformValid(const FTransform& Transform)
{
    return IsVectorFinite(Transform.GetLocation()) && IsVectorFinite(Transform.GetScale3D());
}