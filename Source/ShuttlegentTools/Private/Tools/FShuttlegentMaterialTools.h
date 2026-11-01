#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 材质工具：get_material_parameters（读取），set_material_parameters、create_material_instances、assign_material（写入）。 */
struct FShuttlegentMaterialTools
{
	/** 读取工具，排在其他读取工具之后。 */
	static void AppendRead(TArray<TSharedRef<IShuttlegentTool>>& OutTools);

	/** 写入工具，排在其他写入工具之后。 */
	static void AppendWrite(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
