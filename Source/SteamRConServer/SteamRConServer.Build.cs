// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

using UnrealBuildTool;

public class SteamRConServer : ModuleRules
{
	public SteamRConServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
			);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "Sockets"
            }
			);
	}
}
