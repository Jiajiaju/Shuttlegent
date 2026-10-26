#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** What tools/list tells the model about one editor tool (Shuttlegent.md section 10.2). */
struct FShuttlegentToolDefinition
{
	/** Name the model calls; lowercase letters, digits and underscores, unique among all providers. */
	FString Name;

	/** What the tool does and when to use it, for the model. */
	FString Description;

	/** JSON Schema of the arguments object. */
	TSharedPtr<FJsonObject> InputSchema;

	/** Reads only. A tool that is not read-only asks the write gate before it runs. */
	bool bReadOnly = true;

	/** Write tools: runs as one synchronous editor transaction, undone as a whole when any object fails. */
	bool bTransactional = true;
};
