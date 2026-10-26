#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentExternalMcpType.h"
#include "FShuttlegentMcpHeader.h"

#include "FShuttlegentExternalMcpServer.generated.h"

/** A knowledge or tool service the project declares to the runtime with every session (Shuttlegent.md section 11). */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentExternalMcpServer
{
	GENERATED_BODY()

	/** Server name; the runtime prefixes its tool names with it. Must not be "ue", which the editor endpoint uses. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString Name;

	UPROPERTY(Config, EditAnywhere, Category = "Server")
	EShuttlegentExternalMcpType Type = EShuttlegentExternalMcpType::Http;

	/** Http: absolute URL. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString Url;

	UPROPERTY(Config, EditAnywhere, Category = "Server")
	TArray<FShuttlegentMcpHeader> Headers;

	/** Stdio: absolute path of the executable. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString Command;

	UPROPERTY(Config, EditAnywhere, Category = "Server")
	TArray<FString> Arguments;
};
