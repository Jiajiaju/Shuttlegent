#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentTool.h"

/** 内置工具的公共基类：持有各工具在构造函数中填写的定义。 */
class FShuttlegentToolBase : public IShuttlegentTool
{
protected:
	FShuttlegentToolDefinition Definition {};

public:
	//~ Begin IShuttlegentTool Interface.
	virtual const FShuttlegentToolDefinition& GetDefinition() const override { return Definition; }
	//~ End IShuttlegentTool Interface.
};
