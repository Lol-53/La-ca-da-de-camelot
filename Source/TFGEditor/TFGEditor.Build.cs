using UnrealBuildTool;

public class TFGEditor : ModuleRules
{
	public TFGEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd", "PythonScriptPlugin", "ToolsetRegistry",
			"ModelContextProtocol", "ModelContextProtocolEngine"
		});
	}
}
