#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Outcome of one JSON-RPC request sent to the agent runtime. */
struct FShuttlegentAcpResponse
{
	/** true for a "result" response, false for an "error" response or a local failure. */
	bool bSuccess = false;

	/** The "result" object; may be empty for results that carry no fields. */
	TSharedPtr<FJsonObject> Result;

	/** JSON-RPC error code when bSuccess is false. */
	int32 ErrorCode = 0;

	/** JSON-RPC error message when bSuccess is false. */
	FString ErrorMessage;
};
