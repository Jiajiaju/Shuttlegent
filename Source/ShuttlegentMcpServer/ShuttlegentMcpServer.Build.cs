using UnrealBuildTool;

public class ShuttlegentMcpServer : ModuleRules
{
	public ShuttlegentMcpServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Json",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"HTTPServer",
				"HTTP",
				"Projects",
				"ShuttlegentCore",
			}
			);
	}
}
