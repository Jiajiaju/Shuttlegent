#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 节点图工具：tidy_graph（写入，一个可撤销的事务）。 */
struct FShuttlegentGraphTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
