#pragma once

#include "CoreMinimal.h"

/** Mark animation scenes; each tier only carries the scenes it plays (see Resources/anim/<palette>/mark-anim.json). */
enum class EShuttlegentMarkMotion : uint8
{
	// Rest frame, no animation
	None,

	// Warps drop in and the shuttle is woven pick by pick; empty state
	Intro,

	// Shuttle re-woven in a loop over a faint ghost; while a turn runs
	Working,

	// Quick re-weave when the pointer enters the mark
	Hover,

	// Picks open and press shut once; a turn completed
	Done
};
