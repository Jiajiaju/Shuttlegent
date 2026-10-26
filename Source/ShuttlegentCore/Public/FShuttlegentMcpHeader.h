#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentMcpHeader.generated.h"

/** An HTTP header sent to an external MCP server; its value comes from a variable, so secrets stay out of the config. */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentMcpHeader
{
	GENERATED_BODY()

	/** Header name, e.g. Authorization. */
	UPROPERTY(Config, EditAnywhere, Category = "Header")
	FString Name;

	/** Text before the variable's value, e.g. "Bearer ". */
	UPROPERTY(Config, EditAnywhere, Category = "Header")
	FString Prefix;

	/** Variable holding the value, resolved like a provider key: credential manager, editor environment, development .env file. */
	UPROPERTY(Config, EditAnywhere, Category = "Header")
	FString ValueVariable;
};
