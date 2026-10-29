#include "Settings/FShuttlegentSettingsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#include "FShuttlegentToolsets.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentMcpServerModule.h"
#include "ShuttlegentSettings.h"
#include "Tool/IShuttlegentTool.h"

#define LOCTEXT_NAMESPACE "ShuttlegentSettingsCustomization"

namespace FShuttlegentSettingsCustomizationPrivate
{
	bool IsEnabled(const FString& ToolName)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		return !IsValid(Settings) || !Settings->DisabledTools.ContainsByPredicate([&ToolName](const FString& Disabled) { return Disabled.Equals(ToolName, ESearchCase::IgnoreCase); });
	}

	/** Switches a tool and saves the personal settings; tools/list reads the switch on its next call. */
	void SetEnabled(const FString& ToolName, bool bEnabled)
	{
		UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
		if (!IsValid(Settings)) return;

		Settings->DisabledTools.RemoveAll([&ToolName](const FString& Disabled) { return Disabled.Equals(ToolName, ESearchCase::IgnoreCase); });
		if (!bEnabled) Settings->DisabledTools.Add(ToolName);
		Settings->SaveConfig();
	}

	bool IsToolsetEnabled(const FString& ToolsetName)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		return !IsValid(Settings) || !Settings->DisabledToolsets.ContainsByPredicate([&ToolsetName](const FString& Disabled) { return Disabled.Equals(ToolsetName, ESearchCase::IgnoreCase); });
	}

	/** Switches an engine toolset and saves the personal settings; the toolset tools read the switch on their next call. */
	void SetToolsetEnabled(const FString& ToolsetName, bool bEnabled)
	{
		UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
		if (!IsValid(Settings)) return;

		Settings->DisabledToolsets.RemoveAll([&ToolsetName](const FString& Disabled) { return Disabled.Equals(ToolsetName, ESearchCase::IgnoreCase); });
		if (!bEnabled) Settings->DisabledToolsets.Add(ToolsetName);
		Settings->SaveConfig();
	}

	/** First line of a tool's description, for the tooltip. */
	FText Summary(const FShuttlegentToolDefinition& Definition)
	{
		FString FirstLine = Definition.Description;
		Definition.Description.Split(ShuttlegentEditorSettingsUi::LineBreak, &FirstLine, nullptr);
		return FText::FromString(FirstLine);
	}
}

TSharedRef<IDetailCustomization> FShuttlegentSettingsCustomization::MakeInstance()
{
	return MakeShared<FShuttlegentSettingsCustomization>();
}

void FShuttlegentSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, DisabledTools)));

	TArray<TSharedRef<IShuttlegentTool>> Tools = FShuttlegentMcpServerModule::Get().GetRegisteredTools();
	Tools.Sort([](const TSharedRef<IShuttlegentTool>& Left, const TSharedRef<IShuttlegentTool>& Right) { return Left->GetDefinition().Name < Right->GetDefinition().Name; });

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(ShuttlegentEditorSettingsUi::ToolsCategory);
	for (const TSharedRef<IShuttlegentTool>& Tool : Tools)
	{
		const FShuttlegentToolDefinition& Definition = Tool->GetDefinition();
		const FString ToolName = Definition.Name;
		const FText Kind = Definition.bReadOnly ? LOCTEXT("ReadTool", "读") : LOCTEXT("WriteTool", "写，执行前确认");
		const FText Label = FText::Format(LOCTEXT("ToolLabel", "{0}（{1}）"), FText::FromString(ToolName), Kind);

		Category.AddCustomRow(FText::FromString(ToolName))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(Label)
				.ToolTipText(FShuttlegentSettingsCustomizationPrivate::Summary(Definition))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("SwitchTooltip", "关闭后模型看不到也调用不了这个工具；立即生效"))
				.IsChecked_Lambda([ToolName]() { return FShuttlegentSettingsCustomizationPrivate::IsEnabled(ToolName) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([ToolName](ECheckBoxState State) { FShuttlegentSettingsCustomizationPrivate::SetEnabled(ToolName, State == ECheckBoxState::Checked); })
			];
	}

	CustomizeToolsets(DetailBuilder);
}

void FShuttlegentSettingsCustomization::CustomizeToolsets(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, DisabledToolsets)));

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(ShuttlegentEditorSettingsUi::ToolsetsCategory);
	const TArray<FShuttlegentToolsetInfo> Toolsets = FShuttlegentToolsets::GetToolsets();
	if (Toolsets.IsEmpty())
	{
		Category.AddCustomRow(LOCTEXT("NoToolsets", "没有引擎 Toolset"))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoToolsetsHint", "没有启用的引擎 Toolset。在“编辑 > 插件”中搜索 Toolset 启用需要的插件，重启编辑器后在这里出现。"))
				.AutoWrapText(true)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
		return;
	}

	for (const FShuttlegentToolsetInfo& Toolset : Toolsets)
	{
		const FString ToolsetName = Toolset.Name;
		const bool bBlocked = Toolset.bBlockedByProject;
		const FText Label = bBlocked ? FText::Format(LOCTEXT("BlockedToolsetLabel", "{0}（项目已屏蔽）"), FText::FromString(ToolsetName)) : FText::FromString(ToolsetName);
		const FText Tooltip = bBlocked ? LOCTEXT("BlockedToolsetTooltip", "项目设置 > Shuttlegent > General > Blocked Toolset Names 屏蔽了这个 Toolset，个人无法打开") : LOCTEXT("ToolsetSwitchTooltip", "关闭后模型看不到也调用不了这个 Toolset 的工具；立即生效");

		Category.AddCustomRow(FText::FromString(ToolsetName))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(Label)
				.ToolTipText(FText::FromString(Toolset.Description))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SCheckBox)
				.ToolTipText(Tooltip)
				.IsEnabled(!bBlocked)
				.IsChecked_Lambda([ToolsetName, bBlocked]() { return !bBlocked && FShuttlegentSettingsCustomizationPrivate::IsToolsetEnabled(ToolsetName) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([ToolsetName](ECheckBoxState State) { FShuttlegentSettingsCustomizationPrivate::SetToolsetEnabled(ToolsetName, State == ECheckBoxState::Checked); })
			];
	}
}

#undef LOCTEXT_NAMESPACE
