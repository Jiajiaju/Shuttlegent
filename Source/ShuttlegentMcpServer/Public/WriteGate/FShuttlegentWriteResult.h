#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

#include "WriteGate/FShuttlegentWriteObject.h"

/** What an approved editor write actually did, object by object, as the tool reports it (Shuttlegent UI interaction spec sections 5.2, 5.4). */
struct FShuttlegentWriteResult
{
	int32 RequestId = 0;

	FString Action;

	/** Each object with its real status; the panel never infers them. */
	TArray<FShuttlegentWriteObject> Objects;

	/** The write ran as one synchronous editor transaction. */
	bool bUndoable = false;

	/** That transaction in the editor's undo buffer; invalid when nothing was written or the write is not undoable. */
	FGuid TransactionId;
};
