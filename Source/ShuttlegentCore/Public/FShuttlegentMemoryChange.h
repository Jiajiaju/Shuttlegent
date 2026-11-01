#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentMemoryChangeKind.h"
#include "FShuttlegentMemoryItem.h"

/** 一次记忆修改的前后内容，足以撤销它。 */
struct FShuttlegentMemoryChange
{
	EShuttlegentMemoryChangeKind Kind = EShuttlegentMemoryChangeKind::Added;

	/** 修改前的那一条；新增时为空。 */
	FShuttlegentMemoryItem Before {};

	/** 修改后的那一条；删除时为空。 */
	FShuttlegentMemoryItem After {};
};
