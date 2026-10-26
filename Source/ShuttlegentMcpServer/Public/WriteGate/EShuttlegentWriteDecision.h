#pragma once

#include "CoreMinimal.h"

/** How a pending editor write request left the gate. Each request leaves exactly once. */
enum class EShuttlegentWriteDecision : uint8
{
	// The user confirmed this request; the tool runs it
	Approved,

	// The user refused it, or no confirmation UI was open to ask
	Rejected,

	// Its preview no longer holds (level changed, runtime lost, turn ended); nothing was written
	Expired,

	// Nobody answered within the confirmation time limit, which stays under the runtime's tool call timeout; nothing was written
	TimedOut
};
