#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

#include "Tool/IShuttlegentTool.h"

/**
 * Supplies editor tools to the Shuttlegent MCP endpoint (Shuttlegent.md section 11). Register an instance with
 * IModularFeatures::Get().RegisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider) and
 * unregister it on module shutdown. The endpoint asks every provider for its tools on each tools/list.
 */
class SHUTTLEGENTMCPSERVER_API IShuttlegentToolProvider : public IModularFeature
{
public:
	static FName GetModularFeatureName();

	virtual ~IShuttlegentToolProvider() = default;

	virtual void GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const = 0;
};
