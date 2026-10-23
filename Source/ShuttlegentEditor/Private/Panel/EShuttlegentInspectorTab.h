#pragma once

#include "CoreMinimal.h"

/** Pages of the inspector pane, in tab order. */
enum class EShuttlegentInspectorTab : uint8
{
	// Editor changes waiting for confirmation (filled by the MCP server, step 4)
	Changes,

	// Tool calls of the current session with their parameters and results
	Tools,

	// Events of the current session
	Log
};
