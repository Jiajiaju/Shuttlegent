#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 数据表工具：export_data_table（写入磁盘文件，不可撤销）。 */
struct FShuttlegentDataTableTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
