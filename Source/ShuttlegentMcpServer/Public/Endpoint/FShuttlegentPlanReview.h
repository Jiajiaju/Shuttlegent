#pragma once

#include "CoreMinimal.h"

/** 计划模式下 Agent 交出、等待用户审批的计划；经 FShuttlegentMcpServerModule::AnswerPlanReview 作答。 */
struct FShuttlegentPlanReview
{
	FString SessionId;

	/** session control 插件为这次审批分配的 id。 */
	FString ReviewId;

	/** 计划正文，Markdown，以“# 标题”开头。 */
	FString Plan;
};
