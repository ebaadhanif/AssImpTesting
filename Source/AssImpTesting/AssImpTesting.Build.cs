// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class AssImpTesting : ModuleRules
{
	public AssImpTesting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput","Json" , "JsonUtilities", "ProceduralMeshComponent", "AssetRegistry",
            "MeshDescription", 
    "StaticMeshDescription", "ImageWrapper", "RuntimeModelsImporter"});
        
    }
}
