using UnrealBuildTool;

public class ShuttlegentEditor : ModuleRules
{
	public ShuttlegentEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"DeveloperSettings",
				"ShuttlegentCore",
				"ShuttlegentMcpServer",
			}
			);
	}
}
