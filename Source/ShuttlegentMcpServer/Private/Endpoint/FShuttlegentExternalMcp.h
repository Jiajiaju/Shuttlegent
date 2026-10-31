#pragma once

#include "CoreMinimal.h"

class FJsonValue;
struct FShuttlegentExternalMcpState;

/**
 * ACP mcpServers entries of the project's external MCP servers (Shuttlegent.md section 11). The runtime refuses a
 * session when any declared server fails to start, so entries that cannot work are left out with a warning: no name,
 * the editor endpoint's name, an http entry without a URL, a stdio command that is not an absolute path, or a header
 * whose variable has no value. Header values are resolved like provider keys and never logged.
 */
struct FShuttlegentExternalMcp
{
	static TArray<TSharedPtr<FJsonValue>> MakeDeclarations();

	/** Every server of the project settings in order, with why it is left out; for the panel, logs nothing. */
	static TArray<FShuttlegentExternalMcpState> DescribeServers();
};
