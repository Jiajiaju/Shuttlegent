#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentAgentEntry.generated.h"

/** 一个 Agent 的个人设置；Agent 本身由插件提供，不手填。 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentAgentEntry
{
	GENERATED_BODY()

	/** Agent 的标识，由插件填写。 */
	UPROPERTY(Config, VisibleAnywhere, Category = "Agent")
	FString AgentId;

	/** 是否出现在会话的 Agent 选择器中；默认 Agent 始终启用。 */
	UPROPERTY(Config, EditAnywhere, Category = "Agent")
	bool bEnabled = true;

	/** 选中该 Agent 时带出的模型，按模型名称选择；留空使用默认值中的默认模型。 */
	UPROPERTY(Config, EditAnywhere, Category = "Agent")
	FString DefaultModel;
};
