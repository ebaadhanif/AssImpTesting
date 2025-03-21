//LoadFBXModel("C:/Users/ebaad.hanif/Desktop/FBX Models/Hut.fbx");
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

AMeshLoader::AMeshLoader()
{
    PrimaryActorTick.bCanEverTick = false;
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
    SetRootComponent(StaticMeshComponent);
}

void AMeshLoader::BeginPlay()
{
    Super::BeginPlay();
    LoadFBXModel("C:/Users/ebaad.hanif/Desktop/FBX Models/Car.fbx");
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
        aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
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

    for (unsigned int i = 0; i < Mesh->mNumVertices; i++)
    {
        FVector Position(Mesh->mVertices[i].x, -Mesh->mVertices[i].y, Mesh->mVertices[i].z);
        Position = Transform.TransformPosition(Position);

        FVector Normal(Mesh->mNormals[i].x, -Mesh->mNormals[i].y, Mesh->mNormals[i].z);
        FVector2D UV(0.0f, 0.0f);

        if (Mesh->HasTextureCoords(0))
        {
            UV = FVector2D(Mesh->mTextureCoords[0][i].x, 1.0f - Mesh->mTextureCoords[0][i].y);
        }

        Vertices.Add(Position);
        Normals.Add(Normal);
        UVs.Add(UV);
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

    UStaticMesh* StaticMesh = CreateStaticMesh(Vertices, Triangles, Normals, UVs);
    if (StaticMesh)
    {
        SpawnMeshInScene(StaticMesh, Transform);
    }
}

UStaticMesh* AMeshLoader::CreateStaticMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs)
{
    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
    StaticMesh->InitResources();
    StaticMesh->AddSourceModel();

    FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
    if (!MeshDescription)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateStaticMesh: Failed to create MeshDescription!"));
        return nullptr;
    }

    FStaticMeshAttributes Attributes(*MeshDescription);
    Attributes.Register();

    FPolygonGroupID PolygonGroup = MeshDescription->CreatePolygonGroup();
    TArray<FVertexID> VertexIDs;

    for (const FVector& Vertex : Vertices)
    {
        FVertexID NewVertex = MeshDescription->CreateVertex();
        VertexIDs.Add(NewVertex);
        Attributes.GetVertexPositions()[NewVertex] = FVector3f(Vertex);
    }

    for (int i = 0; i < Triangles.Num(); i += 3)
    {
        FVertexInstanceID V0 = MeshDescription->CreateVertexInstance(VertexIDs[Triangles[i]]);
        FVertexInstanceID V1 = MeshDescription->CreateVertexInstance(VertexIDs[Triangles[i + 1]]);
        FVertexInstanceID V2 = MeshDescription->CreateVertexInstance(VertexIDs[Triangles[i + 2]]);

        MeshDescription->CreateTriangle(PolygonGroup, { V0, V1, V2 });
    }

    StaticMesh->CommitMeshDescription(0);
    StaticMesh->Build();

    return StaticMesh;
}

void AMeshLoader::SpawnMeshInScene(UStaticMesh* StaticMesh, const FTransform& Transform)
{
    AStaticMeshActor* MeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
    MeshActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
    MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    MeshActor->SetActorScale3D(Transform.GetScale3D());
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
