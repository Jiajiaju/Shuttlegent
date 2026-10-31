#pragma once

#include "CoreMinimal.h"

/** How a question the agent asked the user left the question gate. Each question leaves exactly once. */
enum class EShuttlegentQuestionOutcome : uint8
{
	// The user picked an option or wrote an answer
	Answered,

	// The user closed the question without answering
	Skipped,

	// Nobody answered within the time limit, which stays under the runtime's tool call timeout
	TimedOut,

	// The question no longer applies (the turn ended or the runtime was lost)
	Expired,

	// No panel was open to show the question
	NoPanel
};
