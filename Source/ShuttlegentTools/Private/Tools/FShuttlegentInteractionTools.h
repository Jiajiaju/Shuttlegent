#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** Interaction tools: ask_user, a question with a choice of answers that the user answers in the panel. */
struct FShuttlegentInteractionTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
