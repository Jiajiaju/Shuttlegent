#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "Tool/FShuttlegentToolResult.h"

class FShuttlegentWriteGate;
class IShuttlegentTool;

/**
 * Runs one tool call (Shuttlegent.md section 10.3). A read-only tool answers at once. A write tool previews, waits for
 * the write gate, and once confirmed writes inside one editor transaction; when any object fails the transaction is
 * undone as a whole and the written objects are reported as rolled back. The result goes to the gate (the panel's
 * result bar) and, as text, back to the model. Output is cut to the output budget. Game thread only.
 */
class FShuttlegentToolRunner
{
public:
	static void Call(const TSharedRef<FShuttlegentWriteGate>& Gate, const TSharedRef<IShuttlegentTool>& Tool, const TSharedRef<FJsonObject>& Arguments, TFunction<void(const FShuttlegentToolResult&)> OnDone);

	/** Cuts text to the output budget and says so; a paged tool's header still states what it sent before the cut. */
	static FString LimitOutput(const FString& Text);
};
