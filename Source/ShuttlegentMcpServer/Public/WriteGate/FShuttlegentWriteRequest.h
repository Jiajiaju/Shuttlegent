#pragma once

#include "CoreMinimal.h"

#include "WriteGate/FShuttlegentWriteObject.h"

/** An editor write waiting for confirmation (Shuttlegent UI interaction spec section 5.1). Filled by the tool, numbered by the gate. */
struct FShuttlegentWriteRequest
{
	/** Assigned by FShuttlegentWriteGate; the requestId of the interaction spec. */
	int32 RequestId = 0;

	/** Tool that asks, as the model called it. */
	FString ToolName;

	/** What the write does, in words shown on the confirmation card. */
	FString Action;

	/** Level the objects belong to. */
	FString Level;

	/** The tool runs it as one synchronous editor transaction, so it can be undone; false runs it outside any transaction. */
	bool bUndoable = false;

	/** Deletes, moves or renames things; the confirmation card warns about it. */
	bool bDestructive = false;

	TArray<FShuttlegentWriteObject> Objects;

	/** The tool's arguments as JSON, shown on request. */
	FString RawArguments;

	/** Set by the gate: when the request times out unanswered (UTC); the confirmation card counts down to it. */
	FDateTime ExpiresAtUtc {};
};
