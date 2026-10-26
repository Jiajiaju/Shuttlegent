#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** Editor history tools: undo and redo the editor's own transactions, after confirmation. */
struct FShuttlegentHistoryTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
