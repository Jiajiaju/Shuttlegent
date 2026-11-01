#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 记忆工具：remember、forget，修改个人记忆文件 Saved/Shuttlegent/MEMORY.md；不经写入门，面板以一行提示并可撤销。 */
struct FShuttlegentMemoryTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
