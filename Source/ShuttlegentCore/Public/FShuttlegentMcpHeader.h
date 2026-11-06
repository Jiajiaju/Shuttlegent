#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentMcpHeader.generated.h"

/** HTTP header sent to an external MCP server; the value comes from a variable so keys stay out of the config. */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentMcpHeader
{
	GENERATED_BODY()

	/** Header name, for example Authorization. */
	UPROPERTY(Config, EditAnywhere, Category = "Header")
	FString Name;

	/** Text placed before the variable value, for example "Bearer ". */
	UPROPERTY(Config, EditAnywhere, Category = "Header")
	FString Prefix;

	/** Name of the variable that holds the value; each user fills it in under Editor Preferences > Shuttlegent > General > Mcp Header Values. */
	UPROPERTY(Config, EditAnywhere, Category = "Header")
	FString ValueVariable;
};
