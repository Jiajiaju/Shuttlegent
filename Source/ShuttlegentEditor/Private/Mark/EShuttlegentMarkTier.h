#pragma once

#include "CoreMinimal.h"

/** Size tier of the mark animation sprite sheets. */
enum class EShuttlegentMarkTier : uint8
{
	// Empty-state mark, about 128 px, 8 picks
	Large,

	// Header and sidebar mark, about 32 px, 4 picks
	Small
};
