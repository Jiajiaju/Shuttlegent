#pragma once

#include "CoreMinimal.h"

/** Why an external MCP server of the project settings is left out of the runtime's mcpServers declaration. */
enum class EShuttlegentExternalMcpIssue : uint8
{
	// Declared with every session
	None,

	// The name is empty or taken by the editor endpoint
	NameInvalid,

	// Another external server has the same name
	NameTaken,

	// Stdio: the command is not an absolute path
	CommandNotAbsolute,

	// Http: no URL
	UrlMissing,

	// Http: a header's variable has no value
	HeaderValueMissing
};
