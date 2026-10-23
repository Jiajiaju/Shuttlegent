#pragma once

#include "CoreMinimal.h"

/** Layout of the chat panel by its width (Shuttlegent UI interaction spec section 2). */
enum class EShuttlegentPanelLayout : uint8
{
	// Below BreakpointMedium: one column; sidebar and inspector are overlays, the inspector up to the full width
	Narrow,

	// BreakpointMedium to BreakpointWide: centered column; both panes are overlays
	Medium,

	// BreakpointWide to BreakpointFull: inspector docked, sidebar an overlay
	Wide,

	// From BreakpointFull: sidebar and inspector both docked
	Full
};
