#pragma once

#include "CoreMinimal.h"

/** How the last turn of the current conversation ended. */
enum class EShuttlegentTurnOutcome : uint8
{
	// No turn has ended since the conversation was opened or the last prompt was sent
	None,

	// Ended normally
	Completed,

	// Cancelled, or stopped for a reason other than an error
	Stopped,

	// Ended with an error
	Failed
};
