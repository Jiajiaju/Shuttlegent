#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 资源工具：find_assets、get_asset、get_asset_references。仅读取资源注册表，不加载任何内容。 */
struct FShuttlegentAssetTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
