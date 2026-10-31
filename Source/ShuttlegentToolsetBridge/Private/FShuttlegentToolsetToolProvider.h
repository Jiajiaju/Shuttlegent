#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentToolProvider.h"

/** 向 Shuttlegent MCP 端点提供引擎 Toolset 工具（list_toolsets、describe_toolset、call_toolset_tool）。 */
class FShuttlegentToolsetToolProvider : public IShuttlegentToolProvider
{
private:
	TArray<TSharedRef<IShuttlegentTool>> Tools;

public:
	FShuttlegentToolsetToolProvider();

	//~ Begin IShuttlegentToolProvider Interface.
	virtual void GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const override;
	//~ End IShuttlegentToolProvider Interface.
};
