#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentExternalMcpType.generated.h"

/** Transport of an external MCP server that the project declares to the runtime. */
UENUM()
enum class EShuttlegentExternalMcpType : uint8
{
	// Streamable HTTP, reached through a URL
	Http,

	// Local process started by the runtime, talking over stdin and stdout
	Stdio
};
