#pragma once

#include "CoreMinimal.h"

#include "ShuttlegentPermissionPolicy.generated.h"

/** How a session/request_permission from the runtime is answered when no UI handler is bound. */
UENUM()
enum class EShuttlegentPermissionPolicy : uint8
{
	// Reject every request
	Reject,

	// Allow every request once
	Allow
};
