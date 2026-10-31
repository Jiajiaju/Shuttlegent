#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 作用于对象的写入工具：set_property、spawn_actor、delete_actors（各自为一个可撤销的事务）以及 save_assets。 */
struct FShuttlegentEditTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
