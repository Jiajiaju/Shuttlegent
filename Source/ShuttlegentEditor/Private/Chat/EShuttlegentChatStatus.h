#pragma once

#include "CoreMinimal.h"

/** What the panel's header reports for the current conversation (Shuttlegent UI interaction spec section 3). */
enum class EShuttlegentChatStatus : uint8
{
	// No runtime process yet
	NotConnected,

	// Runtime launched, waiting for initialize
	Connecting,

	// Runtime ready and idle; the last turn, if any, completed
	Connected,

	// A turn is running
	Querying,

	// A turn is waiting for the user to answer a runtime permission request
	AwaitingPermission,

	// Stop was sent; waiting for the turn to end
	Stopping,

	// The last turn was stopped by the user or the runtime
	Stopped,

	// The last turn ended with an error
	Failed,

	// The runtime exited unexpectedly
	Lost,

	// A stored session is being resumed in the runtime
	Resuming
};
