#pragma once

#include "CoreMinimal.h"

/** Lifecycle of the agent runtime as seen by FShuttlegentAgentService. */
enum class EShuttlegentAgentState : uint8
{
	// No runtime process
	Stopped,

	// Process launched, waiting for the initialize response
	Starting,

	// Initialized; may or may not have an active session
	Ready,

	// A session/prompt is in flight
	Prompting
};
