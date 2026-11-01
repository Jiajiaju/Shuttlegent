#pragma once

#include "CoreMinimal.h"

/** 个人记忆文件中的一条记忆。 */
struct FShuttlegentMemoryItem
{
	/** 记忆内容，一句话。 */
	FString Text;

	/** 记录日期（YYYY-MM-DD）；手工写入、没有日期时为空。 */
	FString Date;
};
