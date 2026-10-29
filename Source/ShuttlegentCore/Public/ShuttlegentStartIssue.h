#pragma once

#include "CoreMinimal.h"

/** Why the runtime cannot serve prompts, as the panel's start guide card tells it (Shuttlegent.md section 14.3.8). */
enum class EShuttlegentStartIssue : uint8
{
	// Nothing known to be wrong
	None,

	// The runtime executable or its profile patch is not where the plugin expects it
	MissingRuntime,

	// No key for the default model provider in the user's settings
	MissingKey,

	// The provider rejected the key
	AuthFailed,

	// The runtime failed to start or initialize for another reason
	Other
};
