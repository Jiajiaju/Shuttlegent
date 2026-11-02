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
				"InputCore",
				"Projects",
				"ApplicationCore",
				"Settings",
				"Json",
				"ShuttlegentCore",
				"ShuttlegentMcpServer",
				"ShuttlegentToolsetBridge",
				"WebBrowser",
				"AssetRegistry",
				"ContentBrowser",
				"LevelEditor",
				"UnrealEd",
				"RenderCore",
				"PropertyEditor",
				"DesktopPlatform",
				"ImageCore",
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("dwrite.lib");
		}
	}
}
