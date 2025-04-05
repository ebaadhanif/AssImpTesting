// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class AssImpTesting : ModuleRules
{
	public AssImpTesting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ProceduralMeshComponent", "AssetRegistry",
            "MeshDescription", 
    "StaticMeshDescription", "ImageWrapper", "RuntimeModelsImporter"});

        string AssimpPath = Path.Combine(ModuleDirectory, "../../ThirdParty/Assimp");


        PublicIncludePaths.Add(Path.Combine(AssimpPath, "include"));

        PublicAdditionalLibraries.Add(Path.Combine(AssimpPath, "lib", "assimp-vc143-mt.lib"));

        string DLLSourcePath = Path.Combine(AssimpPath, "bin", "assimp-vc143-mt.dll");
        string DLLDestPath = Path.Combine("$(BinaryOutputDir)", "assimp-vc143-mt.dll");

        if (File.Exists(DLLSourcePath))
        {
            RuntimeDependencies.Add(DLLDestPath, DLLSourcePath);
        }
        else
        {
            System.Console.WriteLine("Warning: Assimp DLL not found at " + DLLSourcePath);
        }
    }
}
