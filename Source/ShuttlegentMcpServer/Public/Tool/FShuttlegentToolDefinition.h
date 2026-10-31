#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** tools/list 向模型说明的单个编辑器工具信息（Shuttlegent.md 第 10.2 节）。 */
struct FShuttlegentToolDefinition
{
	/** 模型调用时使用的名称；由小写字母、数字与下划线组成，在所有工具提供者之间唯一。 */
	FString Name;

	/** 面向模型说明工具的作用及使用时机。 */
	FString Description;

	/** 参数对象的 JSON Schema。 */
	TSharedPtr<FJsonObject> InputSchema;

	/** 仅读取。非只读工具在运行前须请求写入门。 */
	bool bReadOnly = true;

	/** 写入工具：作为一个同步编辑器事务运行，任一对象失败时整体撤销。 */
	bool bTransactional = true;
};
