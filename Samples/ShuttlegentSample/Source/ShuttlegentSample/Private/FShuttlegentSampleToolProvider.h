#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentToolProvider.h"

/**
 * 项目自有的编辑器工具，通过 IShuttlegentToolProvider 提供给 Shuttlegent（Shuttlegent.md 第 11 节）：
 * sample_count_actors 只读，sample_set_actor_label 经确认卡片以一个可撤销步骤执行写入。
 */
class FShuttlegentSampleToolProvider : public IShuttlegentToolProvider
{
private:
	TArray<TSharedRef<IShuttlegentTool>> Tools;

public:
	FShuttlegentSampleToolProvider();

	//~ Begin IShuttlegentToolProvider Interface.
	virtual void GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const override;
	//~ End IShuttlegentToolProvider Interface.
};
