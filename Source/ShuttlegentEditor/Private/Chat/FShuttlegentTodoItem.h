#pragma once

#include "CoreMinimal.h"

/** Agent 待办清单中的一项，取自运行时的 todo/write 事件（经 session tap 插件）。 */
struct FShuttlegentTodoItem
{
	FString Content;

	/** pending、in_progress 或 completed。 */
	FString Status;
};
