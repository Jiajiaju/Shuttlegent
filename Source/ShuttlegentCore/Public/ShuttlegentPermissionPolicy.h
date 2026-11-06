#pragma once

#include "CoreMinimal.h"

#include "ShuttlegentPermissionPolicy.generated.h"

/** How to answer session/request_permission from the runtime when no UI handler is bound. */
UENUM()
enum class EShuttlegentPermissionPolicy : uint8
{
	// Reject every request
	Reject,

	// Allow each request once
	Allow
};
