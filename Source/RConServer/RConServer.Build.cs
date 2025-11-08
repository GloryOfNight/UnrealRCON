// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

using UnrealBuildTool;

public class RConServer : ModuleRules
{
    public RConServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "RConCommon"
            }
            );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "Sockets",
                "DeveloperSettings"
            }
            );

        PrivateDefinitions.Add("RCON_SERVER_ALLOW_IN_GAME_SHIPPING=0");
    }
}
