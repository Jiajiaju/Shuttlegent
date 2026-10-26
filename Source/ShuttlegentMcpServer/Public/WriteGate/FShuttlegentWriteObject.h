#pragma once

#include "CoreMinimal.h"

#include "WriteGate/EShuttlegentWriteObjectStatus.h"

/** One object an editor write touches: what changes on it, from what to what, and how it went. */
struct FShuttlegentWriteObject
{
	/** Actor label or asset name. */
	FString Name;

	/** Object path, the stable identity. */
	FString Path;

	/** What changes, e.g. a property name. */
	FString Field;

	/** Value before the write and the planned value, as shown to the user. */
	FString CurrentValue;
	FString PlannedValue;

	EShuttlegentWriteObjectStatus Status = EShuttlegentWriteObjectStatus::NotRun;

	/** Why the write of this object failed or did not run; empty when it completed. */
	FString Message;
};
