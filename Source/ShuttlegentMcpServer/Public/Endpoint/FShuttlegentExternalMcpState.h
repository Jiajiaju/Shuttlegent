#pragma once

#include "CoreMinimal.h"

#include "Endpoint/EShuttlegentExternalMcpIssue.h"

/** One external MCP server of the project settings and whether the runtime gets it with the next session. */
struct FShuttlegentExternalMcpState
{
	FString Name {};

	/** Started by the runtime as a local process; otherwise reached over http. */
	bool bStdio = false;

	EShuttlegentExternalMcpIssue Issue = EShuttlegentExternalMcpIssue::None;

	/** HeaderValueMissing: the header's variable name; empty otherwise. */
	FString Detail {};
};
