#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentWriteMode.generated.h"

/** 对话中编辑器写入的确认方式；计划模式另由运行时记录，不在此列。删除、移动、改名在任何模式下都要确认。 */
UENUM()
enum class EShuttlegentWriteMode : uint8
{
	// 每次写入都弹确认卡
	Manual,

	// 可撤销的写入直接执行，不可撤销的仍要确认
	AcceptEdits,

	// 除删除、移动、改名外的写入都直接执行
	Auto
};
