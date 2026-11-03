#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

/**
 * Editor Preferences > Shuttlegent > General > Providers 中每个供应商条目：标题行显示名称、协议与模型数，
 * 字段照常显示，末尾加“检测连接”行，依次检测条目下的每个模型。M2 改为按模型条目检测后删除本类。
 */
class FShuttlegentProviderEntryCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	//~ Begin IPropertyTypeCustomization Interface.
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization Interface.
};
