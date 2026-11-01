#pragma once

#include "CoreMinimal.h"

/** 对个人记忆文件的一次修改。 */
enum class EShuttlegentMemoryChangeKind : uint8
{
	// 新增一条
	Added,

	// 一条被新的内容替换
	Updated,

	// 删除一条
	Removed
};
