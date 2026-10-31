#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/**
 * 以三个编辑器工具提供引擎 Toolset：list_toolsets 与 describe_toolset 让模型无需全部列出即可在数百个工具中找到所需工具，
 * call_toolset_tool 负责执行其中一个。只读工具立即执行；其他工具均须经过写入门。
 */
struct FShuttlegentToolsetTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
