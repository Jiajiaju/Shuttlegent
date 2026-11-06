#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentExternalMcpType.h"
#include "FShuttlegentMcpHeader.h"

#include "FShuttlegentExternalMcpServer.generated.h"

/** Knowledge or tool server that the project declares to the runtime in every session. */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentExternalMcpServer
{
	GENERATED_BODY()

	/** Server name; the runtime uses it as the prefix of this server's tool names. Must not be "ue", which the editor endpoint uses. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString Name;

	/** Transport: Http is reached through a URL, Stdio is a local process started by the runtime. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	EShuttlegentExternalMcpType Type = EShuttlegentExternalMcpType::Http;

	/** Http: absolute URL. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString Url;

	/** Http: headers sent with each request; values come from variables so keys stay out of the config. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	TArray<FShuttlegentMcpHeader> Headers;

	/** Stdio: absolute path of the executable. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString Command;

	/** Stdio: launch arguments. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	TArray<FString> Arguments;
};
