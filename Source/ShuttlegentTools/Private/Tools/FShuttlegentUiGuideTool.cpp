#include "Tools/FShuttlegentUiGuideTool.h"

#include "Framework/Docking/TabManager.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"

#include "FShuttlegentToolBase.h"
#include "ShuttlegentProjectSettings.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"

namespace FShuttlegentUiGuideToolPrivate
{
	// 工具名称与参数名称
	inline const TCHAR* ToolName = TEXT("show_editor_ui");
	inline const TCHAR* ArgId = TEXT("id");

	// 设置查看器所在的模块，以及设置目标 Container/Category/Section 的分隔符
	inline const TCHAR* SettingsModuleName = TEXT("Settings");
	inline const TCHAR* TargetSeparator = TEXT("/");
	inline constexpr int32 SettingsTargetParts = 3;

	// 由其自身的标签页管理器持有关卡编辑器标签页（World Settings、Outliner、Details）的模块
	inline const TCHAR* LevelEditorModuleName = TEXT("LevelEditor");

	/** 打开指引所指的位置；标签页或页面不存在时返回 false 并填写 OutError。 */
	bool Open(const FShuttlegentUiGuide& Guide, FString& OutError)
	{
		if (Guide.Kind == EShuttlegentUiGuideKind::Tab)
		{
			// Nomad 标签页位于全局标签页管理器中，关卡编辑器自身的标签页位于其自己的标签页管理器中
			const FTabId TabId(FName(*Guide.Target));
			TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(TabId);
			FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(LevelEditorModuleName);
			const TSharedPtr<FTabManager> LevelEditorTabs = LevelEditor != nullptr ? LevelEditor->GetLevelEditorTabManager() : nullptr;
			if (!Tab.IsValid() && LevelEditorTabs.IsValid()) Tab = LevelEditorTabs->TryInvokeTab(TabId);
			if (Tab.IsValid()) return true;

			OutError = FString::Printf(TEXT("The editor has no tab %s."), *Guide.Target);
			return false;
		}

		TArray<FString> Parts;
		Guide.Target.ParseIntoArray(Parts, TargetSeparator);
		ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>(SettingsModuleName);
		if (Parts.Num() != SettingsTargetParts || Settings == nullptr)
		{
			OutError = FString::Printf(TEXT("The settings target %s is not Container/Category/Section."), *Guide.Target);
			return false;
		}

		Settings->ShowViewer(FName(*Parts[0]), FName(*Parts[1]), FName(*Parts[2]));
		return true;
	}

	class FShowEditorUi : public FShuttlegentToolBase
	{
	private:
		TArray<FShuttlegentUiGuide> Guides;

	public:
		explicit FShowEditorUi(const TArray<FShuttlegentUiGuide>& InGuides)
			: Guides(InGuides)
		{
			TArray<FString> Ids;
			TArray<FString> Lines;
			for (const FShuttlegentUiGuide& Guide : Guides)
			{
				Ids.Add(Guide.Id);
				Lines.Add(FString::Printf(TEXT("- %s: %s"), *Guide.Id, *Guide.Description));
			}

			Definition.Name = ToolName;
			Definition.Description = TEXT("Open a place in the editor for the user (a tab or a settings page) when they ask where to find or change something. Places:\n") + FString::Join(Lines, TEXT("\n"));
			Definition.InputSchema = FShuttlegentToolSchema()
				.Enum(ArgId, TEXT("Which place to open."), Ids, true)
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const FString Id = FShuttlegentToolArguments::GetString(Arguments, ArgId);
			const FShuttlegentUiGuide* Guide = Guides.FindByPredicate([&Id](const FShuttlegentUiGuide& Item) { return Item.Id == Id; });
			if (Guide == nullptr) return FShuttlegentToolResult::Error(FString::Printf(TEXT("No UI guide %s."), *Id));

			FString Error;
			if (!Open(*Guide, Error)) return FShuttlegentToolResult::Error(Error);

			return FShuttlegentToolResult::Ok(FString::Printf(TEXT("Opened %s for the user. %s"), *Guide->Target, *Guide->Description));
		}
	};
}

TSharedPtr<IShuttlegentTool> FShuttlegentUiGuideTool::Make()
{
	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	if (!IsValid(Project)) return nullptr;

	TArray<FShuttlegentUiGuide> Guides = Project->UiGuides;
	Guides.RemoveAll([](const FShuttlegentUiGuide& Guide) { return Guide.Id.IsEmpty() || Guide.Target.IsEmpty(); });
	if (Guides.IsEmpty()) return nullptr;

	return MakeShared<FShuttlegentUiGuideToolPrivate::FShowEditorUi>(Guides);
}
