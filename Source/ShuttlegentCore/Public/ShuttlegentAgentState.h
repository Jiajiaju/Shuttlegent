#pragma once

#include "CoreMinimal.h"

/** Lifecycle of the agent runtime as seen by FShuttlegentAgentService. Whether a session runs a prompt is per session (FShuttlegentAgentService::IsPrompting). */
enum class EShuttlegentAgentState : uint8
{
	// No runtime process
	Stopped,

	// Process launched, waiting for the initialize response
	Starting,

	// Initialized; sessions can be created, resumed and prompted
	Ready
};
