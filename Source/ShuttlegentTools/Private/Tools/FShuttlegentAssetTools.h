#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** Asset tools: find_assets, get_asset, get_asset_references. Read the asset registry only; nothing is loaded. */
struct FShuttlegentAssetTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
