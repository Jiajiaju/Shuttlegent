#pragma once

#include "CoreMinimal.h"

/** One entry of the session index kept by Shuttlegent (session/list of the runtime is not relied on). */
struct FShuttlegentSessionRecord
{
	/** ACP session id. */
	FString SessionId;

	/** Display title, taken from the first prompt. */
	FString Title;

	/** Session cwd (the project directory). */
	FString WorkingDirectory;

	/** Model selection as reported by the runtime's config options. */
	FString Model;

	/** Display name of Model, from the same config option; shown in the session list. */
	FString ModelName;

	FDateTime CreatedAt {};

	FDateTime UpdatedAt {};

	/** Hidden from the session list until restored; the runtime session is kept. */
	bool bArchived = false;
};
