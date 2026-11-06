#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentUiGuideKind.generated.h"

/** Kind of place that a UI guide entry opens for the user. */
UENUM()
enum class EShuttlegentUiGuideKind : uint8
{
	// Editor tab, opened by tab ID
	Tab,

	// Settings page, opened by container / category / section
	Settings
};
