#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentTool.h"

/** Common base of the built-in tools: holds the definition each tool fills in its constructor. */
class FShuttlegentToolBase : public IShuttlegentTool
{
protected:
	FShuttlegentToolDefinition Definition {};

public:
	//~ Begin IShuttlegentTool Interface.
	virtual const FShuttlegentToolDefinition& GetDefinition() const override { return Definition; }
	//~ End IShuttlegentTool Interface.
};
