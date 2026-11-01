#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 关卡搭建工具：place_actors 批量新建、arrange_actors 整理已有 Actor（各自为一个可撤销的事务）。 */
struct FShuttlegentLevelBuildTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
