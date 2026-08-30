// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TFG : ModuleRules
{
	public TFG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"PCG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
			"Slate",
			"SlateCore"
		});

		PublicIncludePaths.AddRange(new string[]
		{
			"TFG",
			"TFG/Variant_Platforming",
			"TFG/Variant_Combat",
			"TFG/Variant_Combat/AI",
			"TFG/Variant_SideScrolling",
			"TFG/Variant_SideScrolling/Gameplay",
			"TFG/Variant_SideScrolling/AI"
		});
	}
}
