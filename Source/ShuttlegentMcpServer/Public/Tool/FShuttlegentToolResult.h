#pragma once

#include "CoreMinimal.h"

/** Text a tool call returns to the model; bError marks a call that did not do what was asked. */
struct SHUTTLEGENTMCPSERVER_API FShuttlegentToolResult
{
	FString Text;
	bool bError = false;

	static FShuttlegentToolResult Ok(FString InText);
	static FShuttlegentToolResult Error(FString InText);
};
