#pragma once

#include "CoreMinimal.h"

#include "Reference/FShuttlegentReference.h"

/** One prompt waiting in a session's queue. Text, references and send options are frozen when it is queued (Shuttlegent UI interaction spec section 4.3). */
struct FShuttlegentQueuedPrompt
{
	/** Stable id for removing or taking back the item. */
	int32 Id = 0;

	/** Draft text, without the reference block. */
	FString Text;

	TArray<FShuttlegentReference> References;

	/** Model and reasoning effort selected when the item was queued; empty when the option was not offered. */
	FString ModelValue;
	FString ReasoningValue;
};
