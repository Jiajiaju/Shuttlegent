#pragma once

#include "CoreMinimal.h"

/** What happened to a draft the user sent. */
enum class EShuttlegentSubmitResult : uint8
{
	// Sent as a prompt (or held until the runtime and the session are ready)
	Sent,

	// A turn is running: appended to the session's queue
	Queued,

	// Not taken: empty text, a reference that no longer exists, or the runtime is lost
	Rejected
};
