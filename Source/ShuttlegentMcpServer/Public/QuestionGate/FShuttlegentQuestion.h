#pragma once

#include "CoreMinimal.h"

/** A question the agent asks the user with a choice of answers (ask_user). Filled by the tool, numbered by the gate. */
struct FShuttlegentQuestion
{
	/** Assigned by FShuttlegentQuestionGate. */
	int32 QuestionId = 0;

	FString Question;

	/** The answers offered, in the order shown. */
	TArray<FString> Options;

	/** The user may write an answer of their own instead of picking an option. */
	bool bAllowOther = true;

	/** Set by the gate: when the question times out unanswered (UTC); the card counts down to it. */
	FDateTime ExpiresAtUtc {};
};
