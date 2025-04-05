// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class RuntimeModelsImporter : ModuleRules
{
	public RuntimeModelsImporter(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // Unreal modules your plugin depends on
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore",
            "EnhancedInput", "ProceduralMeshComponent",
            "AssetRegistry", "RenderCore", "RHI", "MeshDescription",
            "StaticMeshDescription", "ImageWrapper"
        });

        // ✅ Third-party Assimp setup
	string AssimpPath = Path.Combine(ModuleDirectory, "ThirdParty", "Assimp");
       // string AssimpPath = Path.Combine(ModuleDirectory, "../../ThirdParty/Assimp");


        // Assimp includes
        PublicIncludePaths.Add(Path.Combine(AssimpPath, "include"));

        // Assimp lib
        PublicAdditionalLibraries.Add(Path.Combine(AssimpPath, "lib", "assimp-vc143-mt.lib"));

        // Assimp DLL runtime copy for packaged builds
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

        // Load DLL at runtime (delayed)
        PublicDelayLoadDLLs.Add("assimp-vc143-mt.dll");

        // Required for Assimp
        bUseRTTI = true;
    }
}
