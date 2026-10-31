#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Editor Preferences > Shuttlegent > General：将 UShuttlegentSettings::DisabledTools 显示为每个已注册
 * 编辑器工具（内置、项目与示例工具均同）一个开关，因此后注册的工具默认为开启；并将
 * DisabledToolsets 显示为每个引擎 Toolset 一个开关；被项目屏蔽的 Toolset 显示为关闭且锁定。
 */
class FShuttlegentSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface.
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization Interface.

private:
	void CustomizeToolsets(IDetailLayoutBuilder& DetailBuilder);
};
