#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentUiGuideKind.generated.h"

/** What a UI guide entry opens for the user. */
UENUM()
enum class EShuttlegentUiGuideKind : uint8
{
	// An editor tab, by its tab id
	Tab,

	// A settings page, by container / category / section
	Settings
};
