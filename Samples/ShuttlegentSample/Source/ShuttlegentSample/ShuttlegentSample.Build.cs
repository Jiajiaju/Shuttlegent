using UnrealBuildTool;

public class ShuttlegentSample : ModuleRules
{
	public ShuttlegentSample(ReadOnlyTargetRules Target) : base(Target)
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
				"UnrealEd",
				"Json",
				"ShuttlegentMcpServer",
			}
			);
	}
}
