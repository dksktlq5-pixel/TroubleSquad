// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TroubleSquad : ModuleRules
{
	public TroubleSquad(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "MoviePlayer", "OnlineSubsystem", "RenderCore" });
        AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
	}
}
