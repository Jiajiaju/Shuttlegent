#pragma once

#include "CoreMinimal.h"

/** What happened to one object of an editor write (Shuttlegent UI interaction spec section 5.2). */
enum class EShuttlegentWriteObjectStatus : uint8
{
	// Not written yet: the request waits for confirmation, or a batch stopped before this object
	NotRun,

	// Written
	Completed,

	// The write of this object failed; the message says why
	Failed,

	// Written, then reverted with the whole transaction because another object of the batch failed
	RolledBack
};
