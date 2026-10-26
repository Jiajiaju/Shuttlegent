#pragma once

#include "CoreMinimal.h"

#include "Reference/EShuttlegentReferenceKind.h"

/** One reference chip of the draft (Shuttlegent UI interaction spec section 4.2). Path is the stable identity; Name is only shown. */
struct FShuttlegentReference
{
	EShuttlegentReferenceKind Kind = EShuttlegentReferenceKind::Asset;

	/** Asset object path, or the actor's object path in the editor world. */
	FString Path;

	/** Asset name or actor label. */
	FString Name;

	/** Asset class or actor class name. */
	FString ClassName;
};
