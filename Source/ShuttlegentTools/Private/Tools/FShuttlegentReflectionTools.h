#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 反射工具：get_properties、describe_class、get_settings。 */
struct FShuttlegentReflectionTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
