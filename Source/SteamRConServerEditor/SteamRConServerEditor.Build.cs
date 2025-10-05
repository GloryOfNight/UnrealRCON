// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

using UnrealBuildTool;

public class SteamRConServerEditor : ModuleRules
{
    public SteamRConServerEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "SteamRConServer"
            }
            );
    }
}
