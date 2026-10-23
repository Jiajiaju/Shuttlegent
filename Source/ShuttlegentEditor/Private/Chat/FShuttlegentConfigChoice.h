#pragma once

#include "CoreMinimal.h"

/** One selectable value of an ACP session config option (model, reasoning effort). */
struct FShuttlegentConfigChoice
{
	/** Value sent back with session/set_config_option. */
	FString Value;

	/** Display name. */
	FString Name;
};
