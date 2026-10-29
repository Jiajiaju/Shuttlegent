#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/**
 * Engine toolsets as three editor tools: list_toolsets and describe_toolset let the model find a tool among hundreds
 * without listing them all, call_toolset_tool runs one. Read tools run at once; every other tool goes through the write gate.
 */
struct FShuttlegentToolsetTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
