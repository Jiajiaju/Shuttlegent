#include "Settings/FShuttlegentSettingsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
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

	/** 预设下拉：每项写入属性句柄，经属性变更通知让面板实时换字体；空值显示为"默认"。 */
	TSharedRef<SWidget> MakeFontPresetMenu(TSharedRef<IPropertyHandle> Handle, const TArray<FString>& Presets)
	{
		FMenuBuilder Menu(true, nullptr);
		for (const FString& Preset : Presets)
		{
			const FText Label = Preset.IsEmpty() ? LOCTEXT("DefaultFont", "默认") : FText::FromString(Preset);
			Menu.AddMenuEntry(Label, FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([Handle, Preset]() { Handle->SetValue(Preset); })));
		}
		return Menu.MakeWidget();
	}

	/** 一项字体设置：可直接输入的文本框，右侧一个预设按钮。 */
	void CustomizeFontRow(IDetailLayoutBuilder& DetailBuilder, FName PropertyName, const TArray<FString>& Presets)
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
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					Handle->CreatePropertyValueWidget(false)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(ShuttlegentEditorSettingsUi::FontPresetGap, 0.0f, 0.0f, 0.0f)
				[
					SNew(SComboButton)
					.ToolTipText(LOCTEXT("FontPresetTooltip", "从常用字体中选择；本机没有安装的字体会自动退回默认"))
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FontPreset", "预设"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
					.OnGetMenuContent_Lambda([Handle, Presets]() { return MakeFontPresetMenu(Handle, Presets); })
				]
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
	CustomizeFonts(DetailBuilder);
}

void FShuttlegentSettingsCustomization::CustomizeFonts(IDetailLayoutBuilder& DetailBuilder)
{
	const TArray<FString> TextPresets = FShuttlegentSettingsCustomizationPrivate::ToPresets(ShuttlegentEditorSettingsUi::TextFontPresets);
	const TArray<FString> MonoPresets = FShuttlegentSettingsCustomizationPrivate::ToPresets(ShuttlegentEditorSettingsUi::MonoFontPresets);

	FShuttlegentSettingsCustomizationPrivate::CustomizeFontRow(DetailBuilder, GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, UiFontFamily), TextPresets);
	FShuttlegentSettingsCustomizationPrivate::CustomizeFontRow(DetailBuilder, GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, ChatFontFamily), TextPresets);
	FShuttlegentSettingsCustomizationPrivate::CustomizeFontRow(DetailBuilder, GET_MEMBER_NAME_CHECKED(UShuttlegentSettings, CodeFontFamily), MonoPresets);
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
