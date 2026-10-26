#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentExternalMcpType.generated.h"

/** Transport of an external MCP server the project declares to the runtime. */
UENUM()
enum class EShuttlegentExternalMcpType : uint8
{
	// Streamable HTTP at a URL
	Http,

	// A local process the runtime starts, talking over stdin and stdout
	Stdio
};
