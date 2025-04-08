#include "ModelsConfigManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

void UModelsConfigManager::LoadConfig(FString FilePath)
{
    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to load config file: %s"), *FilePath);
        return;
    }

    TArray<TSharedPtr<FJsonValue>> ModelsArray;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);

    if (!FJsonSerializer::Deserialize(Reader, ModelsArray))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to parse config JSON as array"));
        return;
    }

    for (const TSharedPtr<FJsonValue>& ModelValue : ModelsArray)
    {
        TSharedPtr<FJsonObject> ModelObj = ModelValue->AsObject();
        if (!ModelObj.IsValid()) continue;

        FModelAttachmentConfig Config;
        ModelObj->TryGetStringField("ModelName", Config.ModelName);
        ModelObj->TryGetStringField("ModelID", Config.ModelID);

        const TArray<TSharedPtr<FJsonValue>>* Attachments;
        if (ModelObj->TryGetArrayField("Attachments", Attachments))
        {
            for (const TSharedPtr<FJsonValue>& AttachmentValue : *Attachments)
            {
                TSharedPtr<FJsonObject> AttachmentObj = AttachmentValue->AsObject();
                if (!AttachmentObj.IsValid()) continue;

                FAttachmentConfig Attach;
                AttachmentObj->TryGetStringField("NodeName", Attach.NodeName);
                AttachmentObj->TryGetStringField("AttachmentType", Attach.AttachmentType);
                AttachmentObj->TryGetStringField("AssetPath", Attach.AssetPath);
                Config.Attachments.Add(Attach);
            }
        }

        ModelConfigs.Add(Config);
    }

    UE_LOG(LogTemp, Display, TEXT("✅ Loaded config for %d models"), ModelConfigs.Num());
}



void UModelsConfigManager::AttachConfigToModel(UAssimpRuntime3DModelsImporter* Loader)
{
    if (!Loader) return;

    const FString ModelName = Loader->GetModelName();

    for (const FModelAttachmentConfig& Config : ModelConfigs)
    {
        if (Config.ModelName == ModelName)
        {
            Loader->SetModelID(Config.ModelID);

            for (const FAttachmentConfig& Attachment : Config.Attachments)
            {
                AActor* NodeActor = Loader->GetNodeActorByName(Attachment.NodeName);
                if (!NodeActor)
                {
                    UE_LOG(LogTemp, Warning, TEXT("⚠️ Node not found: %s in model %s"), *Attachment.NodeName, *ModelName);
                    continue;
                }

                AttachElementToNode(Attachment, NodeActor);
            }

            break; // done with this model
        }
    }
}

void UModelsConfigManager::AttachElementToNode(const FAttachmentConfig& Attachment, AActor* NodeActor)
{
    if (!NodeActor) return;

    UWorld* World = NodeActor->GetWorld();
    if (!World) return;

    // Use the node's location/rotation
    FVector Location = NodeActor->GetActorLocation();
    FRotator Rotation = NodeActor->GetActorRotation();

    if (Attachment.AttachmentType == "StaticMesh")
    {
        UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Attachment.AssetPath));
        if (Mesh)
        {
            AActor* MeshActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation);
            UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(MeshActor);
            MeshComp->SetStaticMesh(Mesh);
            MeshComp->RegisterComponent();
            MeshActor->SetRootComponent(MeshComp);
            MeshActor->AttachToActor(NodeActor, FAttachmentTransformRules::KeepRelativeTransform);
            UE_LOG(LogTemp, Display, TEXT("✅ Attached StaticMesh Actor to node %s"), *Attachment.NodeName);
        }
    }
    else if (Attachment.AttachmentType == "VFX")
    {
        // Uncomment and use if you have VFX systems
        /*
        UParticleSystem* VFX = Cast<UParticleSystem>(StaticLoadObject(UParticleSystem::StaticClass(), nullptr, *Attachment.AssetPath));
        if (VFX)
        {
            AActor* VFXActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation);
            UParticleSystemComponent* VFXComp = NewObject<UParticleSystemComponent>(VFXActor);
            VFXComp->SetTemplate(VFX);
            VFXComp->RegisterComponent();
            VFXActor->SetRootComponent(VFXComp);
            VFXActor->AttachToActor(NodeActor, FAttachmentTransformRules::KeepRelativeTransform);
            UE_LOG(LogTemp, Display, TEXT("✅ Attached VFX Actor to node %s"), *Attachment.NodeName);
        }
        */
    }
    else if (Attachment.AttachmentType == "Blueprint")
    {
        UClass* BPClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *Attachment.AssetPath));
        if (BPClass)
        {
            AActor* BPActor = World->SpawnActor<AActor>(BPClass, Location, Rotation);
            if (BPActor)
            {
                BPActor->AttachToActor(NodeActor, FAttachmentTransformRules::KeepRelativeTransform);
                UE_LOG(LogTemp, Display, TEXT("✅ Spawned Blueprint Actor at node %s"), *Attachment.NodeName);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Unknown AttachmentType: %s"), *Attachment.AttachmentType);
    }
}
