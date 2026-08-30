using UnrealBuildTool;

public class TFGTests : ModuleRules
{
	public TFGTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AssetRegistry",
			"UnrealEd",
			"Kismet",
			"BlueprintGraph",
			"TFG"
		});
	}
}
