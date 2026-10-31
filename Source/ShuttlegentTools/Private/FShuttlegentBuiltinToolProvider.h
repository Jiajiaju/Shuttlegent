#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentToolProvider.h"

class FShuttlegentLogCapture;

/** 适用于任意 UE 项目的通用编辑器工具（Shuttlegent.md 第 10.2 节）。 */
class FShuttlegentBuiltinToolProvider : public IShuttlegentToolProvider
{
private:
	TArray<TSharedRef<IShuttlegentTool>> Tools;

public:
	explicit FShuttlegentBuiltinToolProvider(const TSharedRef<FShuttlegentLogCapture>& LogCapture);

	//~ Begin IShuttlegentToolProvider Interface.
	virtual void GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const override;
	//~ End IShuttlegentToolProvider Interface.
};
