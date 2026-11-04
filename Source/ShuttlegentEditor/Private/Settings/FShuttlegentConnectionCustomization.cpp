#include "Settings/FShuttlegentConnectionCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "FShuttlegentModelRoutes.h"
#include "Settings/SShuttlegentProbeRow.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentSettings.h"

#define LOCTEXT_NAMESPACE "ShuttlegentConnectionCustomization"

namespace FShuttlegentConnectionCustomizationPrivate
{
	/** 句柄指向的连接在 UShuttlegentSettings::Connections 中的下标；多选、句柄失效或不是设置中的连接时为 INDEX_NONE。 */
	int32 GetIndex(const TSharedRef<IPropertyHandle>& Handle)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		if (!IsValid(Settings) || !Handle->IsValidHandle()) return INDEX_NONE;

		TArray<const void*> RawData;
		Handle->AccessRawData(RawData);
		if (RawData.Num() != 1 || Settings->Connections.IsEmpty()) return INDEX_NONE;

		const int32 Index = static_cast<int32>(static_cast<const FShuttlegentConnection*>(RawData[0]) - Settings->Connections.GetData());
		return Settings->Connections.IsValidIndex(Index) ? Index : INDEX_NONE;
	}

	/** 标题行摘要：地址（内置连接为 DeepSeek），已连通时加协议。 */
	FText Summary(const TSharedRef<IPropertyHandle>& Handle)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		const int32 Index = GetIndex(Handle);
		if (Index == INDEX_NONE) return FText::GetEmpty();

		const FShuttlegentConnection& Connection = Settings->Connections[Index];
		const FString Url = FShuttlegentModelRoutes::TrimBaseUrl(Connection.BaseUrl);
		FString Text = Connection.bBuiltin ? FString(TEXT("DeepSeek (built-in)")) : (Url.IsEmpty() ? FString(TEXT("New connection")) : Url);
		if (Connection.Status == EShuttlegentConnectionStatus::Connected)
		{
			Text += ShuttlegentEditorSettingsUi::ProbeFieldSeparator + FString(FShuttlegentModelRoutes::GetApiName(Connection.Api));
		}
		return FText::FromString(Text);
	}

	/** 已连通时检测出的全部模型，最新的在前；未连通时为空，该行隐藏。 */
	FText ModelList(const TSharedRef<IPropertyHandle>& Handle)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		const int32 Index = GetIndex(Handle);
		if (Index == INDEX_NONE || Settings->Connections[Index].Status != EShuttlegentConnectionStatus::Connected) return FText::GetEmpty();

		return FText::FromString(FString::Join(Settings->Connections[Index].ModelIds, ShuttlegentEditorSettingsUi::ModelListSeparator));
	}

	bool IsBuiltin(const TSharedRef<IPropertyHandle>& Handle)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		const int32 Index = GetIndex(Handle);
		return Index != INDEX_NONE && Settings->Connections[Index].bBuiltin;
	}
}

TSharedRef<IPropertyTypeCustomization> FShuttlegentConnectionCustomization::MakeInstance()
{
	return MakeShared<FShuttlegentConnectionCustomization>();
}

void FShuttlegentConnectionCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// 数组条目的菜单（插入、复制、删除）由引擎加在标题行末尾，这里不再加；内置连接被删后由 UShuttlegentSettings::EnsureBuiltins 加回
	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text_Lambda([PropertyHandle]() { return FShuttlegentConnectionCustomizationPrivate::Summary(PropertyHandle); })
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

void FShuttlegentConnectionCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const bool bBuiltin = FShuttlegentConnectionCustomizationPrivate::IsBuiltin(PropertyHandle);

	// 只露出地址与密钥；协议与模型由检测得出，不在这里编辑
	const TSharedPtr<IPropertyHandle> BaseUrl = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FShuttlegentConnection, BaseUrl));
	const TSharedPtr<IPropertyHandle> ApiKey = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FShuttlegentConnection, ApiKey));
	if (BaseUrl.IsValid()) ChildBuilder.AddProperty(BaseUrl.ToSharedRef()).IsEnabled(!bBuiltin);
	if (ApiKey.IsValid()) ChildBuilder.AddProperty(ApiKey.ToSharedRef());

	ChildBuilder.AddCustomRow(LOCTEXT("ProbeSearch", "Test connection"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ProbeName", "Status"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(ShuttlegentEditorSettingsUi::ProbeValueMinWidth)
		[
			SNew(SShuttlegentProbeRow)
			.ConnectionIndex_Lambda([PropertyHandle]() { return FShuttlegentConnectionCustomizationPrivate::GetIndex(PropertyHandle); })
		];

	ChildBuilder.AddCustomRow(LOCTEXT("ModelsSearch", "Models"))
		.Visibility(TAttribute<EVisibility>::CreateLambda([PropertyHandle]() { return FShuttlegentConnectionCustomizationPrivate::ModelList(PropertyHandle).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; }))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ModelsName", "Models"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(ShuttlegentEditorSettingsUi::ProbeValueMinWidth)
		[
			SNew(STextBlock)
			.Text_Lambda([PropertyHandle]() { return FShuttlegentConnectionCustomizationPrivate::ModelList(PropertyHandle); })
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
}

#undef LOCTEXT_NAMESPACE
