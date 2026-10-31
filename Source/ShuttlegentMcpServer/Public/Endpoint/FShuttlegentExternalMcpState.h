#pragma once

#include "CoreMinimal.h"

#include "Endpoint/EShuttlegentExternalMcpIssue.h"

/** 项目设置中的一个外部 MCP 服务器，以及运行时能否在下一个会话中获得它。 */
struct FShuttlegentExternalMcpState
{
	FString Name {};

	/** 由运行时作为本地进程启动；否则通过 http 访问。 */
	bool bStdio = false;

	EShuttlegentExternalMcpIssue Issue = EShuttlegentExternalMcpIssue::None;

	/** HeaderValueMissing 时为该请求头引用的变量名；其他情况为空。 */
	FString Detail {};
};
