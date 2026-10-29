using UnrealBuildTool;

public class ShuttlegentToolsetBridge : ModuleRules
{
	public ShuttlegentToolsetBridge(ReadOnlyTargetRules Target) : base(Target)
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
				"EditorSubsystem",
				"Json",
				"UnrealEd",
				"ToolsetRegistry",
				"ShuttlegentCore",
				"ShuttlegentMcpServer",
			}
			);
	}
}
