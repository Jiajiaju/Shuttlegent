#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentToolProvider.h"

/** Offers the engine toolset tools (list_toolsets, describe_toolset, call_toolset_tool) to the Shuttlegent MCP endpoint. */
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
