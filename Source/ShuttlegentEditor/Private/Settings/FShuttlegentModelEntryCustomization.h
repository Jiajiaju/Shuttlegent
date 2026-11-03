#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

/**
 * Editor Preferences > Shuttlegent > General > Models 中每个模型：标题行显示名称与协议；内置 DeepSeek 模型锁定名称、模型 ID 与协议，
 * 不可删除；密钥留空时提示沿用谁的密钥；可把密钥同步到同一地址的其他模型；末尾一行“检测连接”。
 */
class FShuttlegentModelEntryCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	//~ Begin IPropertyTypeCustomization Interface.
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization Interface.
};
