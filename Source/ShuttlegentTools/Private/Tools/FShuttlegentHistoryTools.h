#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 编辑器历史工具：经确认后撤销或重做编辑器自身的事务。 */
struct FShuttlegentHistoryTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
