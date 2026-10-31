#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentExternalMcpType.generated.h"

/** 项目声明给运行时的外部 MCP 服务所用的传输方式。 */
UENUM()
enum class EShuttlegentExternalMcpType : uint8
{
	// 通过 URL 访问的 Streamable HTTP
	Http,

	// 由运行时启动的本机进程，经 stdin 与 stdout 通信
	Stdio
};
