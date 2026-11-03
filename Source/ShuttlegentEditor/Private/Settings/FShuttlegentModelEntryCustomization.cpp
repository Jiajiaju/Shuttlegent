#include "Settings/FShuttlegentModelEntryCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "FShuttlegentModelProbe.h"
#include "FShuttlegentModelRoutes.h"
#include "Settings/SShuttlegentProbeRow.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentSettings.h"

#define LOCTEXT_NAMESPACE "ShuttlegentModelEntryCustomization"

namespace FShuttlegentModelEntryCustomizationPrivate
{
	/** 句柄指向的模型在 UShuttlegentSettings::Models 中的下标；多选、句柄失效或不是设置中的模型时为 INDEX_NONE。 */
	int32 GetIndex(const TSharedRef<IPropertyHandle>& Handle)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		if (!IsValid(Settings) || !Handle->IsValidHandle()) return INDEX_NONE;

		TArray<const void*> RawData;
		Handle->AccessRawData(RawData);
		if (RawData.Num() != 1 || Settings->Models.IsEmpty()) return INDEX_NONE;

		const int32 Index = static_cast<int32>(static_cast<const FShuttlegentModelEntry*>(RawData[0]) - Settings->Models.GetData());
		return Settings->Models.IsValidIndex(Index) ? Index : INDEX_NONE;
	}

	/** 标题行摘要：名称 · 协议；内置模型标为内置 DeepSeek。 */
	FText Summary(const TSharedRef<IPropertyHandle>& Handle)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		const int32 Index = GetIndex(Handle);
		if (Index == INDEX_NONE) return FText::GetEmpty();

		const FShuttlegentModelEntry& Entry = Settings->Models[Index];
		const FText Name = Entry.Name.IsEmpty() ? LOCTEXT("Unnamed", "未命名") : FText::FromString(Entry.Name);
		const FText Kind = Entry.bBuiltin ? LOCTEXT("Builtin", "内置 DeepSeek") : StaticEnum<EShuttlegentProviderApi>()->GetDisplayNameTextByValue(static_cast<int64>(Entry.Api));
		return FText::Format(LOCTEXT("Summary", "{0} · {1}"), Name, Kind);
	}

	/** 密钥留空时说明沿用了谁的密钥，或没有可沿用的；自己填了密钥时为空。 */
	FText KeyHint(const TSharedRef<IPropertyHandle>& Handle)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		const int32 Index = GetIndex(Handle);
		if (Index == INDEX_NONE || !Settings->Models[Index].ApiKey.TrimStartAndEnd().IsEmpty()) return FText::GetEmpty();

		int32 Source = INDEX_NONE;
		FShuttlegentModelRoutes::ResolveKey(*Settings, Index, Source);
		if (Source == INDEX_NONE) return LOCTEXT("NoKey", "未填写密钥，检测与对话都会失败");
		return FText::Format(LOCTEXT("InheritedKey", "未填写，沿用“{0}”的密钥（地址与协议相同）"), FText::FromString(Settings->Models[Source].Name));
	}

	/** 地址与协议相同、密钥与本模型不同的其他自定义模型；本模型没填密钥或是内置模型时为空。 */
	TArray<int32> KeySyncTargets(int32 Index)
	{
		TArray<int32> Targets;
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		if (!IsValid(Settings) || !Settings->Models.IsValidIndex(Index)) return Targets;

		const FShuttlegentModelEntry& Entry = Settings->Models[Index];
		const FString Key = Entry.ApiKey.TrimStartAndEnd();
		if (Entry.bBuiltin || Key.IsEmpty()) return Targets;

		for (int32 OtherIndex = 0; OtherIndex < Settings->Models.Num(); ++OtherIndex)
		{
			const FShuttlegentModelEntry& Other = Settings->Models[OtherIndex];
			if (OtherIndex == Index || Other.bBuiltin || Other.ApiKey.TrimStartAndEnd() == Key) continue;
			if (FShuttlegentModelRoutes::IsSameEndpoint(Entry.BaseUrl, Entry.Api, Other.BaseUrl, Other.Api)) Targets.Add(OtherIndex);
		}
		return Targets;
	}

	/** 把本模型的密钥写给地址与协议相同的其他模型，并保存个人设置。 */
	void SyncKey(int32 Index)
	{
		UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
		const TArray<int32> Targets = KeySyncTargets(Index);
		if (!IsValid(Settings) || Targets.IsEmpty()) return;

		const FString Key = Settings->Models[Index].ApiKey.TrimStartAndEnd();
		for (const int32 Target : Targets)
		{
			Settings->Models[Target].ApiKey = Key;
		}
		Settings->SaveConfig();
	}

	/** 与其他模型重名时的提示；不重名时为空。 */
	FText DuplicateName(const TSharedRef<IPropertyHandle>& Handle)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		const int32 Index = GetIndex(Handle);
		if (Index == INDEX_NONE || Settings->Models[Index].Name.IsEmpty()) return FText::GetEmpty();

		const FString& Name = Settings->Models[Index].Name;
		bool bDuplicate = false;
		for (int32 OtherIndex = 0; OtherIndex < Settings->Models.Num(); ++OtherIndex)
		{
			bDuplicate |= OtherIndex != Index && Settings->Models[OtherIndex].Name == Name;
		}
		return bDuplicate ? LOCTEXT("DuplicateName", "与其他模型重名：默认模型与 Agent 的默认模型只会选中排在前面的那个") : FText::GetEmpty();
	}

	/** 一行说明文字，内容为空时隐藏。 */
	void AddNoteRow(IDetailChildrenBuilder& ChildBuilder, const FText& SearchText, TFunction<FText()> GetText, const FSlateColor& Color)
	{
		ChildBuilder.AddCustomRow(SearchText)
			.Visibility(TAttribute<EVisibility>::CreateLambda([GetText]() { return GetText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; }))
			.ValueContent()
			.MinDesiredWidth(ShuttlegentEditorSettingsUi::ProbeValueMinWidth)
			[
				SNew(STextBlock)
				.Text_Lambda([GetText]() { return GetText(); })
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(Color)
				.AutoWrapText(true)
			];
	}
}

TSharedRef<IPropertyTypeCustomization> FShuttlegentModelEntryCustomization::MakeInstance()
{
	return MakeShared<FShuttlegentModelEntryCustomization>();
}

void FShuttlegentModelEntryCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	const int32 Index = FShuttlegentModelEntryCustomizationPrivate::GetIndex(PropertyHandle);
	const bool bBuiltin = Index != INDEX_NONE && Settings->Models[Index].bBuiltin;

	// 内置模型不给数组条目按钮，不能删除或复制
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
				.Text_Lambda([PropertyHandle]() { return FShuttlegentModelEntryCustomizationPrivate::Summary(PropertyHandle); })
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				bBuiltin ? SNullWidget::NullWidget : PropertyHandle->CreateDefaultPropertyButtonWidgets()
			]
		];
}

void FShuttlegentModelEntryCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	const int32 Index = FShuttlegentModelEntryCustomizationPrivate::GetIndex(PropertyHandle);
	const bool bBuiltin = Index != INDEX_NONE && Settings->Models[Index].bBuiltin;

	// 内置模型的名称、模型 ID 与协议由运行时决定，只读显示
	const FName LockedFields[] = { GET_MEMBER_NAME_CHECKED(FShuttlegentModelEntry, Name), GET_MEMBER_NAME_CHECKED(FShuttlegentModelEntry, ModelId) };
	for (const FName Field : LockedFields)
	{
		const TSharedPtr<IPropertyHandle> Child = PropertyHandle->GetChildHandle(Field);
		if (Child.IsValid()) ChildBuilder.AddProperty(Child.ToSharedRef()).IsEnabled(!bBuiltin);
	}

	FShuttlegentModelEntryCustomizationPrivate::AddNoteRow(ChildBuilder, LOCTEXT("DuplicateSearch", "重名"), [PropertyHandle]() { return FShuttlegentModelEntryCustomizationPrivate::DuplicateName(PropertyHandle); }, FStyleColors::Error);

	const TSharedPtr<IPropertyHandle> BaseUrl = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FShuttlegentModelEntry, BaseUrl));
	const TSharedPtr<IPropertyHandle> Api = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FShuttlegentModelEntry, Api));
	const TSharedPtr<IPropertyHandle> ApiKey = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FShuttlegentModelEntry, ApiKey));
	if (BaseUrl.IsValid()) ChildBuilder.AddProperty(BaseUrl.ToSharedRef());
	if (Api.IsValid()) ChildBuilder.AddProperty(Api.ToSharedRef()).IsEnabled(!bBuiltin);
	if (ApiKey.IsValid()) ChildBuilder.AddProperty(ApiKey.ToSharedRef());

	FShuttlegentModelEntryCustomizationPrivate::AddNoteRow(ChildBuilder, LOCTEXT("KeyHintSearch", "密钥"), [PropertyHandle]() { return FShuttlegentModelEntryCustomizationPrivate::KeyHint(PropertyHandle); }, FSlateColor::UseSubduedForeground());

	const TSharedPtr<IPropertyUtilities> Utilities = CustomizationUtils.GetPropertyUtilities();
	ChildBuilder.AddCustomRow(LOCTEXT("SyncKeySearch", "同步密钥"))
		.Visibility(TAttribute<EVisibility>::CreateLambda([PropertyHandle]()
		{
			const int32 Current = FShuttlegentModelEntryCustomizationPrivate::GetIndex(PropertyHandle);
			return FShuttlegentModelEntryCustomizationPrivate::KeySyncTargets(Current).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
		}))
		.ValueContent()
		[
			SNew(SButton)
			.Text_Lambda([PropertyHandle]()
			{
				const int32 Current = FShuttlegentModelEntryCustomizationPrivate::GetIndex(PropertyHandle);
				return FText::Format(LOCTEXT("SyncKey", "同步密钥到同一地址的其他 {0} 个模型"), FText::AsNumber(FShuttlegentModelEntryCustomizationPrivate::KeySyncTargets(Current).Num()));
			})
			.ToolTipText(LOCTEXT("SyncKeyTooltip", "把这个模型的密钥写给地址与协议相同、密钥与它不同的其他模型（含留空的）"))
			.OnClicked_Lambda([PropertyHandle, Utilities]()
			{
				FShuttlegentModelEntryCustomizationPrivate::SyncKey(FShuttlegentModelEntryCustomizationPrivate::GetIndex(PropertyHandle));
				if (Utilities.IsValid()) Utilities->ForceRefresh();
				return FReply::Handled();
			})
		];

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
				const UShuttlegentSettings* Current = GetDefault<UShuttlegentSettings>();
				const int32 ModelIndex = FShuttlegentModelEntryCustomizationPrivate::GetIndex(PropertyHandle);
				return ModelIndex != INDEX_NONE ? TArray<FShuttlegentModelProbeTarget> { FShuttlegentModelProbe::MakeTarget(*Current, ModelIndex) } : TArray<FShuttlegentModelProbeTarget>();
			})
		];
}

#undef LOCTEXT_NAMESPACE
