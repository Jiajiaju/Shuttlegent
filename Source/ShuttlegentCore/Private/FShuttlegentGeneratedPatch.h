#pragma once

#include "CoreMinimal.h"

/**
 * The second profile patch, written into DSH_HOME before every launch (Shuttlegent.md section 8.2): the project's
 * provider routes and default route and model, its skill directories, and the user's Shell switch. Written as JSON,
 * which the runtime reads as YAML. Holds no secrets: providers name their key variables only.
 */
struct FShuttlegentGeneratedPatch
{
	/** Writes the patch to DshHome; false with OutError when the file cannot be written. */
	static bool Write(const FString& DshHome, bool bShell, FString& OutPath, FString& OutError);
};
