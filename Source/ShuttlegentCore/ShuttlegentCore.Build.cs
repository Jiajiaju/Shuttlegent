using UnrealBuildTool;

public class ShuttlegentCore : ModuleRules
{
	public ShuttlegentCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);
	}
}
