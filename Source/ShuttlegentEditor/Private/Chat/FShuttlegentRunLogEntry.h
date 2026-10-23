#pragma once

#include "CoreMinimal.h"

/** One event of the current conversation shown in the inspector's run log. Never holds runtime stderr, keys or environment. */
struct FShuttlegentRunLogEntry
{
	/** When the event happened (UTC). */
	FDateTime Time {};

	FText Text;

	/** Cancellation, failure or disconnection; shown in the warning color. */
	bool bWarning = false;
};
