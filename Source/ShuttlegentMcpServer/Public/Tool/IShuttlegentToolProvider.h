#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

#include "Tool/IShuttlegentTool.h"

/**
 * 向 Shuttlegent MCP 端点提供编辑器工具（Shuttlegent.md 第 11 节）。通过
 * IModularFeatures::Get().RegisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider) 注册实例，
 * 并在模块关闭时注销。端点在每次 tools/list 时向所有工具提供者获取其工具。
 */
class SHUTTLEGENTMCPSERVER_API IShuttlegentToolProvider : public IModularFeature
{
public:
	static FName GetModularFeatureName();

	virtual ~IShuttlegentToolProvider() = default;

	virtual void GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const = 0;
};
