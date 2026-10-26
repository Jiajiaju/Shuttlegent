#include "Settings/FShuttlegentSettingsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

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
}

#undef LOCTEXT_NAMESPACE
