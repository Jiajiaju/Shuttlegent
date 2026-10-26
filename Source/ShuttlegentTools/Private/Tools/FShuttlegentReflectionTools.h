#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** Reflection tools: get_properties, describe_class, get_settings. */
struct FShuttlegentReflectionTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
