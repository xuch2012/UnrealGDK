// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

using UnrealBuildTool;
using System.IO;

public class SpatialGDKEditorToolbar : ModuleRules
{
    public SpatialGDKEditorToolbar(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bFasterWithoutUnity = true;

        PublicIncludePaths.AddRange(
            new string[] {
                "SpatialGDKEditorToolbar/Public",
                "SpatialGDK/Public"
            }
        );
				
        PrivateIncludePaths.AddRange(
            new string[] 
            {
                "SpatialGDKEditorToolbar/Private",
            }
        );

		PublicDependencyModuleNames.AddRange(
            new string[] 
            {
                "Core",
                "Json",
                "JsonUtilities"
            }
        );

        // Add the SpatialOS Platform dll
		string CSharpPlatfromSDK = Path.GetFullPath("E:\\Projects\\UnrealGDKTestSuite\\Game\\Binaries\\ThirdParty\\Improbable\\CSharpPlatfromSDK\\Improbable.SpatialOS.Platform.dll");
		//PublicAdditionalLibraries.AddRange(new[] { CSharpPlatfromSDK });
		//RuntimeDependencies.Add(CSharpPlatfromSDK, StagedFileType.NonUFS);
		//PublicLibraryPaths.Add(CSharpPlatfromSDK);
		PublicDelayLoadDLLs.Add(CSharpPlatfromSDK);

		PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "LevelEditor",
                "Projects",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "MessageLog",
                "SpatialGDK",
                "UnrealEd"
            }
        );
    }
}
