#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentWriteMode.generated.h"

/** How editor writes in a conversation are confirmed; plan mode is tracked by the runtime and is not listed here. Deleting, moving and renaming always need confirmation. */
UENUM()
enum class EShuttlegentWriteMode : uint8
{
	// Show a confirmation card for every write
	Manual,

	// Run undoable writes directly; writes that cannot be undone still need confirmation
	AcceptEdits,

	// Run every write directly except deleting, moving and renaming
	Auto
};
