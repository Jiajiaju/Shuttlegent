#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

class FShuttlegentWriteGate;

/**
 * Shuttlegent.WriteProbe: a minimal synchronous write tool that goes through FShuttlegentWriteGate exactly as the MCP
 * endpoint's write tools will (Shuttlegent.md section 14.3.6). It asks to move the selected actors up, and once
 * confirmed moves them inside one editor transaction and reports each actor's real outcome. An optional index makes
 * that actor fail and leaves the rest unrun, to show a partial completion.
 */
class FShuttlegentWriteProbe
{
private:
	TWeakPtr<FShuttlegentWriteGate> Gate;
	TUniquePtr<FAutoConsoleCommand> Command;

public:
	explicit FShuttlegentWriteProbe(const TSharedRef<FShuttlegentWriteGate>& InGate);

private:
	void HandleCommand(const TArray<FString>& Args);
};
