#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** Level tools: list_actors, get_selection, on the level being edited. */
struct FShuttlegentLevelTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
