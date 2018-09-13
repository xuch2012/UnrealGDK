// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

using System;
using System.ComponentModel;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using UnrealBuildTool;

public class SpatialGDKEditor : ModuleRules
{
    public SpatialGDKEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[]
            {
                "SpatialGDKEditor/Public",
            });

        PrivateIncludePaths.Add("SpatialGDKEditor/Private");

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "EngineSettings",
                "OnlineSubsystemUtils",
                "PhysXVehicles",
                "InputCore",
                "Sockets",
                "SpatialGDK",
                "UnrealEd",
            });
	}
}
