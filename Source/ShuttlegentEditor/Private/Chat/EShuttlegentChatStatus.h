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

	// An editor write waits for the user to confirm it
	AwaitingWriteConfirmation,

	// A confirmed editor write is running
	Writing,

	// Stop was sent; waiting for the turn to end
	Stopping,

	// The last turn was stopped by the user or the runtime
	Stopped,

	// The last turn ended with an error
	Failed,

	// The runtime exited unexpectedly
	Lost,

	// A stored session is being resumed in the runtime
	Resuming,

	// The runtime cannot start or serve prompts (missing runtime or key, rejected key); the start guide card says why
	StartBlocked,

	// The runtime could not resume the session's context; its local record is kept
	ResumeFailed
};
