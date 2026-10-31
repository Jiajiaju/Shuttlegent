#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 交互工具：ask_user，提出一个带备选答案的问题，由用户在面板中作答。 */
struct FShuttlegentInteractionTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
