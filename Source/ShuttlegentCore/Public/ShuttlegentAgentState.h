#pragma once

#include "CoreMinimal.h"

/** FShuttlegentAgentService 视角下 Agent 运行时的生命周期。会话是否在执行提问按会话区分（FShuttlegentAgentService::IsPrompting）。 */
enum class EShuttlegentAgentState : uint8
{
	// 没有运行时进程
	Stopped,

	// 进程已启动，等待 initialize 答复
	Starting,

	// 已初始化；可以新建、恢复会话并提问
	Ready
};
