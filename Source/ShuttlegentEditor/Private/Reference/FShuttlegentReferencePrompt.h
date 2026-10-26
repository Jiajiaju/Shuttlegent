#pragma once

#include "CoreMinimal.h"

struct FShuttlegentReference;

/**
 * How references travel with a prompt: appended to the text as a block of one line per reference, so the runtime,
 * the transcript and a resumed session all see the same frozen snapshot. Pure functions.
 */
struct FShuttlegentReferencePrompt
{
	/** The draft text followed by the reference block; the text alone when there are no references. */
	static FString Compose(const FString& Text, const TArray<FShuttlegentReference>& References);

	/** Splits a sent prompt back into its text and the lines of its reference block (empty when it has none). */
	static void Split(const FString& Prompt, FString& OutText, TArray<FString>& OutReferenceLines);
};
