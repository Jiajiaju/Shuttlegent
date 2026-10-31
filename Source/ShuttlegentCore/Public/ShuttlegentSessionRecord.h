#pragma once

#include "CoreMinimal.h"

/** Shuttlegent 自行维护的会话索引中的一条（不依赖运行时的 session/list）。 */
struct FShuttlegentSessionRecord
{
	/** ACP 会话 ID。 */
	FString SessionId;

	/** 显示标题，取自首条提问。 */
	FString Title;

	/** 会话的 cwd（项目目录）。 */
	FString WorkingDirectory;

	/** 运行时配置选项报告的所选模型。 */
	FString Model;

	/** Model 的显示名，取自同一配置选项；显示在会话列表中。 */
	FString ModelName;

	FDateTime CreatedAt {};

	FDateTime UpdatedAt {};

	/** 恢复前不在会话列表中显示；运行时会话仍保留。 */
	bool bArchived = false;
};
