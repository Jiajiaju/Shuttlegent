#pragma once

#include "CoreMinimal.h"

/** What a draft reference points at. */
enum class EShuttlegentReferenceKind : uint8
{
	// An asset, identified by its object path; found through AssetRegistry without loading it
	Asset,

	// An actor of the editor world, identified by its object path
	Actor
};
