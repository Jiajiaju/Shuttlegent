#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentAgentManifest.h"

/**
 * 可用 Agent 的目录：默认循环在前，其后为插件 Runtime/plugins/agents/<id>/agent.json 声明的循环插件
 * （ShuttlegentAgentModel_Design.md 第 9.1 节）。首次使用时扫描并缓存；说明文件无效时跳过并写警告。仅限游戏线程。
 */
struct SHUTTLEGENTCORE_API FShuttlegentAgentCatalog
{
	/** 全部可用的 Agent。 */
	static const TArray<FShuttlegentAgentManifest>& GetAgents();

	/** 按标识找 Agent；找不到时返回空指针。 */
	static const FShuttlegentAgentManifest* Find(const FString& AgentId);

	/** 重新扫描插件目录。 */
	static void Rescan();
};
