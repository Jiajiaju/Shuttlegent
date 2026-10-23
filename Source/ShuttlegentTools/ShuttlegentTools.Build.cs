using UnrealBuildTool;

public class ShuttlegentTools : ModuleRules
{
	public ShuttlegentTools(ReadOnlyTargetRules Target) : base(Target)
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
				"AssetRegistry",
				"ShuttlegentMcpServer",
			}
			);
	}
}
