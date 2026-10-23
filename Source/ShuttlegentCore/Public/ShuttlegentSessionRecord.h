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

	FDateTime CreatedAt {};

	FDateTime UpdatedAt {};
};
