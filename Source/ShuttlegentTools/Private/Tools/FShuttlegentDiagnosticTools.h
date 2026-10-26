#pragma once

#include "CoreMinimal.h"

class FShuttlegentLogCapture;
class IShuttlegentTool;

/** Diagnostic tools: read_log (from the module's log capture) and get_cvar. */
struct FShuttlegentDiagnosticTools
{
	static void Append(const TSharedRef<FShuttlegentLogCapture>& LogCapture, TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
