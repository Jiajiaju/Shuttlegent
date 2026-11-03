#include "Settings/FShuttlegentAgentEntryCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"

#include "FShuttlegentAgentCatalog.h"
#include "Settings/FShuttlegentSettingsChoices.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentSettings.h"

#define LOCTEXT_NAMESPACE "ShuttlegentAgentEntryCustomization"

namespace FShuttlegentAgentEntryCustomizationPrivate
{
	/** 句柄指向的 Agent 标识；多选或句柄失效时为空。 */
	FString GetAgentId(const TSharedRef<IPropertyHandle>& Handle)
	{
		FString AgentId;
		const TSharedPtr<IPropertyHandle> Child = Handle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FShuttlegentAgentEntry, AgentId));
		if (Child.IsValid()) Child->GetValue(AgentId);
		return AgentId;
	}
}

TSharedRef<IPropertyTypeCustomization> FShuttlegentAgentEntryCustomization::MakeInstance()
{
	return MakeShared<FShuttlegentAgentEntryCustomization>();
}

void FShuttlegentAgentEntryCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const FString AgentId = FShuttlegentAgentEntryCustomizationPrivate::GetAgentId(PropertyHandle);
	const FShuttlegentAgentManifest* Manifest = FShuttlegentAgentCatalog::Find(AgentId);
	const FText Description = AgentId == ShuttlegentAgentConstant::DefaultAgentId ? LOCTEXT("DefaultLoop", "运行时自带的默认循环") : FText::FromString(Manifest != nullptr ? Manifest->Description : AgentId);
	HeaderRow
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FShuttlegentSettingsChoices::GetAgentLabel(AgentId))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text(Description)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

void FShuttlegentAgentEntryCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const FString AgentId = FShuttlegentAgentEntryCustomizationPrivate::GetAgentId(PropertyHandle);

	ChildBuilder.AddCustomRow(LOCTEXT("CapabilitiesSearch", "能力"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Capabilities", "能力"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(ShuttlegentEditorSettingsUi::ProbeValueMinWidth)
		[
			SNew(STextBlock)
			.Text(FShuttlegentSettingsChoices::GetAgentCapabilities(AgentId))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.AutoWrapText(true)
		];

	// 默认 Agent 始终启用
	const TSharedPtr<IPropertyHandle> Enabled = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FShuttlegentAgentEntry, bEnabled));
	if (Enabled.IsValid()) ChildBuilder.AddProperty(Enabled.ToSharedRef()).IsEnabled(AgentId != ShuttlegentAgentConstant::DefaultAgentId);

	const TSharedPtr<IPropertyHandle> DefaultModel = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FShuttlegentAgentEntry, DefaultModel));
	if (!DefaultModel.IsValid()) return;

	ChildBuilder.AddProperty(DefaultModel.ToSharedRef())
		.CustomWidget()
		.NameContent()
		[
			DefaultModel->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(ShuttlegentEditorSettingsUi::ProbeValueMinWidth)
		[
			FShuttlegentSettingsChoices::MakeModelCombo(DefaultModel.ToSharedRef(), LOCTEXT("FollowDefault", "跟随默认模型"))
		];
}

#undef LOCTEXT_NAMESPACE
