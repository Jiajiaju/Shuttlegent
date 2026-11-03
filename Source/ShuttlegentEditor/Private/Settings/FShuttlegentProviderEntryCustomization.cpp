#include "Settings/FShuttlegentProviderEntryCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "FShuttlegentModelProbe.h"
#include "FShuttlegentProviderEntry.h"
#include "Settings/SShuttlegentProbeRow.h"
#include "ShuttlegentEditorConstants.h"

#define LOCTEXT_NAMESPACE "ShuttlegentProviderEntryCustomization"

namespace FShuttlegentProviderEntryCustomizationPrivate
{
	/** 句柄指向的条目；多选或句柄已失效时为空。 */
	const FShuttlegentProviderEntry* GetEntry(const TSharedRef<IPropertyHandle>& Handle)
	{
		if (!Handle->IsValidHandle()) return nullptr;

		TArray<const void*> RawData;
		Handle->AccessRawData(RawData);
		return RawData.Num() == 1 ? static_cast<const FShuttlegentProviderEntry*>(RawData[0]) : nullptr;
	}

	/** 标题行摘要：名称 · 协议 · 模型数。 */
	FText Summary(const TSharedRef<IPropertyHandle>& Handle)
	{
		const FShuttlegentProviderEntry* Entry = GetEntry(Handle);
		if (Entry == nullptr) return FText::GetEmpty();

		const FString Name = Entry->DisplayName.IsEmpty() ? Entry->Route : Entry->DisplayName;
		const FText Api = StaticEnum<EShuttlegentProviderApi>()->GetDisplayNameTextByValue(static_cast<int64>(Entry->Api));
		return FText::Format(LOCTEXT("Summary", "{0} · {1} · {2} 个模型"), FText::FromString(Name), Api, FText::AsNumber(Entry->Models.Num()));
	}
}

TSharedRef<IPropertyTypeCustomization> FShuttlegentProviderEntryCustomization::MakeInstance()
{
	return MakeShared<FShuttlegentProviderEntryCustomization>();
}

void FShuttlegentProviderEntryCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([PropertyHandle]() { return FShuttlegentProviderEntryCustomizationPrivate::Summary(PropertyHandle); })
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PropertyHandle->CreateDefaultPropertyButtonWidgets()
			]
		];
}

void FShuttlegentProviderEntryCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 ChildCount = 0;
	PropertyHandle->GetNumChildren(ChildCount);
	for (uint32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		const TSharedPtr<IPropertyHandle> Child = PropertyHandle->GetChildHandle(ChildIndex);
		if (Child.IsValid()) ChildBuilder.AddProperty(Child.ToSharedRef());
	}

	ChildBuilder.AddCustomRow(LOCTEXT("ProbeSearch", "检测连接"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ProbeName", "检测连接"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(ShuttlegentEditorSettingsUi::ProbeValueMinWidth)
		[
			SNew(SShuttlegentProbeRow)
			.OnGetTargets_Lambda([PropertyHandle]()
			{
				const FShuttlegentProviderEntry* Entry = FShuttlegentProviderEntryCustomizationPrivate::GetEntry(PropertyHandle);
				return Entry != nullptr ? FShuttlegentModelProbe::MakeEntryTargets(*Entry) : TArray<FShuttlegentModelProbeTarget>();
			})
		];
}

#undef LOCTEXT_NAMESPACE
