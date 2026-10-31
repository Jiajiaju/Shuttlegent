#pragma once

#include "CoreMinimal.h"

/** 一行捕获到的编辑器日志。 */
struct FShuttlegentLogLine
{
	FDateTime Time {};
	FName Category {};
	ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
	FString Text;
};
