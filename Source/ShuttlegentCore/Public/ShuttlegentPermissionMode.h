#pragma once

#include "CoreMinimal.h"

#include "ShuttlegentPermissionMode.generated.h"

/** Sandbox preset of the DeepSeek Harness runtime (DSH_PERMISSION_MODE). */
UENUM()
enum class EShuttlegentPermissionMode : uint8
{
	// Files can be read; writes need an escalation that the user approves
	ReadOnly,

	// Files inside the workspace can be written without asking
	WorkspaceWrite
};
