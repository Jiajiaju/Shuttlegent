#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentExternalMcpType.h"
#include "FShuttlegentMcpHeader.h"

#include "FShuttlegentExternalMcpServer.generated.h"

/** 项目在每个会话中声明给运行时的知识或工具服务（Shuttlegent.md 第 11 节）。 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentExternalMcpServer
{
	GENERATED_BODY()

	/** 服务名，运行时以它作为该服务工具名的前缀；不得为编辑器端点占用的 "ue"。 */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString Name;

	/** 传输方式：Http 通过 URL 访问，Stdio 由运行时启动本机进程。 */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	EShuttlegentExternalMcpType Type = EShuttlegentExternalMcpType::Http;

	/** Http：绝对 URL。 */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString Url;

	/** Http：随请求发送的请求头；值来自变量，密钥不写进配置。 */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	TArray<FShuttlegentMcpHeader> Headers;

	/** Stdio：可执行文件的绝对路径。 */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString Command;

	/** Stdio：启动参数。 */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	TArray<FString> Arguments;
};
