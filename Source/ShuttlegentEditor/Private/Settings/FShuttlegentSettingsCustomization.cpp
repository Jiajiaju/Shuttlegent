#include "Settings/FShuttlegentSettingsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "FShuttlegentToolsets.h"
#include "Settings/FShuttlegentSettingsChoices.h"
#include "Settings/SShuttlegentFontField.h"
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

	/** 切换某个工具的开关并保存个人设置；tools/list 在下次调用时读取该开关。 */
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

	/** 切换某个引擎 Toolset 的开关并保存个人设置；Toolset 工具在下次调用时读取该开关。 */
	void SetToolsetEnabled(const FString& ToolsetName, bool bEnabled)
	{
		UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
		if (!IsValid(Settings)) return;

		Settings->DisabledToolsets.RemoveAll([&ToolsetName](const FString& Disabled) { return Disabled.Equals(ToolsetName, ESearchCase::IgnoreCase); });
		if (!bEnabled) Settings->DisabledToolsets.Add(ToolsetName);
		Settings->SaveConfig();
	}

	/** 工具描述的第一行，用作提示信息。 */
	FText Summary(const FShuttlegentToolDefinition& Definition)
	{
		FString FirstLine = Definition.Description;
		Definition.Description.Split(ShuttlegentEditorSettingsUi::LineBreak, &FirstLine, nullptr);
		return FText::FromString(FirstLine);
	}

	/** 常量表中的预设字体名转为字符串数组。 */
	TArray<FString> ToPresets(TConstArrayView<const TCHAR*> Names)
	{
		TArray<FString> Presets;
		for (const TCHAR* Name : Names)
		{
			Presets.Add(Name);
		}
		return Presets;
	}

	/** 一项字体设置：单个输入框，点进去列出本机字体；值经属性句柄写入，属性变更通知让面板实时换字体。 */
	void CustomizeFontRow(IDetailLayoutBuilder& DetailBuilder, FName PropertyName, const TArray<FString>& Recommended, bool bMonospaceOnly, const FText& DefaultLabel)
	{
		const TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(PropertyName);
		IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(Handle);
		if (Row == nullptr) return;

		Row->CustomWidget()
			.NameContent()
			[
				Handle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(ShuttlegentEditorSettingsUi::FontValueMinWidth)
			[
				SNew(SShuttlegentFontField)
				.Value_Lambda([Handle]()
				{
					FString Value;
					Handle->GetValue(Value);
					return Value;
				})
				.Recommended(Recommended)
				.bMonospaceOnly(bMonospaceOnly)
				.DefaultLabel(DefaultLabel)
				.OnCommitted_Lambda([Handle](const FString& Value) { Handle->SetValue(Value); })
			];
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
		const FText Kind = Definition.bReadOnly ? LOCTEXT("ReadTool", "Read") : LOCTEXT("WriteTool", "Write, confirmed before running");
		const FText Label = FText::Format(LOCTEXT("ToolLabel", "{0} ({1})"), FText::FromString(ToolName), Kind);

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
				.ToolTipText(LOCTEXT("SwitchTooltip", "When off, the model can neither see nor call this tool; takes effect immediately"))
				.IsChecked_Lambda([ToolName]() { return FShuttlegentSettingsCustomizationPrivate::IsEnabled(ToolName) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([ToolName](ECheckBoxState State) { FShuttlegentSettingsCustomizationPrivate::SetEnabled(ToolName, State == ECheckBoxState::Checked); })
			];
	}

	CustomizeDefaults(DetailBuilder);
	CustomizeToolsets(DetailBuilder);
	CustomizeFonts(DetailBuilder);
}

void FShuttlegentSettingsCustomization::CustomizeDefaults(IDetailLayoutBuilder& DetailBuilder)
{
	// 默认 Agent 从下拉中选，不手填；默认模型自动取第一条已连通的连接中的第一个模型，不设选项
	const TSharedRef<IPropertyHandle> Agent = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, DefaultAgent));
	const TPair<TSharedRef<IPropertyHandle>, TSharedRef<SWidget>> Rows[] = {
		{ Agent, FShuttlegentSettingsChoices::MakeAgentCombo(Agent) },
	};
	for (const TPair<TSharedRef<IPropertyHandle>, TSharedRef<SWidget>>& Row : Rows)
	{
		IDetailPropertyRow* PropertyRow = DetailBuilder.EditDefaultProperty(Row.Key);
		if (PropertyRow == nullptr) continue;

		PropertyRow->CustomWidget()
			.NameContent()
			[
				Row.Key->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(ShuttlegentEditorSettingsUi::ProbeValueMinWidth)
			[
				Row.Value
			];
	}
}

void FShuttlegentSettingsCustomization::CustomizeFonts(IDetailLayoutBuilder& DetailBuilder)
{
	const TArray<FString> TextPresets = FShuttlegentSettingsCustomizationPrivate::ToPresets(ShuttlegentEditorSettingsUi::TextFontPresets);
	const TArray<FString> MonoPresets = FShuttlegentSettingsCustomizationPrivate::ToPresets(ShuttlegentEditorSettingsUi::MonoFontPresets);

	FShuttlegentSettingsCustomizationPrivate::CustomizeFontRow(DetailBuilder, GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, UiFontFamily), TextPresets, false, LOCTEXT("DefaultUiFont", "Default (Segoe UI + Microsoft YaHei UI)"));
	FShuttlegentSettingsCustomizationPrivate::CustomizeFontRow(DetailBuilder, GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, ChatFontFamily), TextPresets, false, LOCTEXT("DefaultChatFont", "Default (follow the UI font)"));
	FShuttlegentSettingsCustomizationPrivate::CustomizeFontRow(DetailBuilder, GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, CodeFontFamily), MonoPresets, true, LOCTEXT("DefaultCodeFont", "Default (Cascadia Mono + Consolas)"));
}

void FShuttlegentSettingsCustomization::CustomizeToolsets(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, DisabledToolsets)));

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(ShuttlegentEditorSettingsUi::ToolsetsCategory);
	const TArray<FShuttlegentToolsetInfo> Toolsets = FShuttlegentToolsets::GetToolsets();
	if (Toolsets.IsEmpty())
	{
		Category.AddCustomRow(LOCTEXT("NoToolsets", "No engine toolsets"))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoToolsetsHint", "No engine toolsets are enabled. Search for Toolset in Edit > Plugins, enable the plugins you need, and they appear here after the editor restarts."))
				.AutoWrapText(true)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
		return;
	}

	for (const FShuttlegentToolsetInfo& Toolset : Toolsets)
	{
		const FString ToolsetName = Toolset.Name;
		const bool bBlocked = Toolset.bBlockedByProject;
		const FText Label = bBlocked ? FText::Format(LOCTEXT("BlockedToolsetLabel", "{0} (blocked by the project)"), FText::FromString(ToolsetName)) : FText::FromString(ToolsetName);
		const FText Tooltip = bBlocked ? LOCTEXT("BlockedToolsetTooltip", "This toolset is blocked by Project Settings > Shuttlegent > General > Blocked Toolset Names and cannot be turned on personally") : LOCTEXT("ToolsetSwitchTooltip", "When off, the model can neither see nor call the tools of this toolset; takes effect immediately");

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
