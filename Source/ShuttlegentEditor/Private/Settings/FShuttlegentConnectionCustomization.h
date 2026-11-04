#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

/**
 * Editor Preferences > Shuttlegent > General > Models 中每条模型连接：标题行显示地址、协议与模型数；只露出地址与密钥两项，
 * 末尾一行检测（SShuttlegentProbeRow）。内置 DeepSeek 连接地址只读、不可删除。
 */
class FShuttlegentConnectionCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	//~ Begin IPropertyTypeCustomization Interface.
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization Interface.
};
