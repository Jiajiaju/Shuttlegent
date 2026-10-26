#pragma once

#include "CoreMinimal.h"

#include "Chat/FShuttlegentQueuedPrompt.h"

/** FIFO of one session's prompts sent while a turn was running. A paused queue waits for "continue queue" (Shuttlegent UI interaction spec section 4.3). */
struct FShuttlegentPromptQueue
{
	TArray<FShuttlegentQueuedPrompt> Items;

	/** Set by a stop, a rejected permission, a failed turn or a lost runtime; cleared only by the user. */
	bool bPaused = false;
};
