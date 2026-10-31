#pragma once

#include "CoreMinimal.h"

#include "WriteGate/EShuttlegentWriteObjectStatus.h"

/** 编辑器写入涉及的单个对象：其上变更的内容、变更前后的值以及执行结果。 */
struct FShuttlegentWriteObject
{
	/** Actor 标签或资产名。 */
	FString Name;

	/** 对象路径，作为稳定标识。 */
	FString Path;

	/** 变更的内容，例如属性名。 */
	FString Field;

	/** 写入前的值与计划值，按向用户显示的形式表示。 */
	FString CurrentValue;
	FString PlannedValue;

	EShuttlegentWriteObjectStatus Status = EShuttlegentWriteObjectStatus::NotRun;

	/** 该对象写入失败或未执行的原因；对已完成的对象，为附注，例如“仍在运行”。 */
	FString Message;

	/** 写入返回给模型的内容，例如引擎 Toolset 工具的 JSON 结果；不在卡片上显示。 */
	FString Output;
};
