#pragma once

#include "CoreMinimal.h"

/** One captured editor log line. */
struct FShuttlegentLogLine
{
	FDateTime Time {};
	FName Category {};
	ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
	FString Text;
};
