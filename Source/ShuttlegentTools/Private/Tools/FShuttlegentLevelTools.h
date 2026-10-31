#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 关卡工具：list_actors、get_selection，作用于正在编辑的关卡。 */
struct FShuttlegentLevelTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
