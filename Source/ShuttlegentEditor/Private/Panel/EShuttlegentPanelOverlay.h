#pragma once

#include "CoreMinimal.h"

/** Pane shown above the conversation; the two overlays exclude each other. */
enum class EShuttlegentPanelOverlay : uint8
{
	None,

	Sidebar,

	Inspector
};
