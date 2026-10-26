#include "Tool/FShuttlegentToolResult.h"

FShuttlegentToolResult FShuttlegentToolResult::Ok(FString InText)
{
	return FShuttlegentToolResult { MoveTemp(InText), false };
}

FShuttlegentToolResult FShuttlegentToolResult::Error(FString InText)
{
	return FShuttlegentToolResult { MoveTemp(InText), true };
}
