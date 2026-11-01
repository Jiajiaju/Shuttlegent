#pragma once

#include "CoreMinimal.h"

/** 写入门对一个新请求的处理方式，由对话模型按当前写入模式给出。 */
enum class EShuttlegentWritePolicy : uint8
{
	// 弹确认卡，等用户答复
	Ask,

	// 不弹确认卡，直接批准执行；结果照常进入对话并可撤销
	Approve,

	// 不征询用户即拒绝（计划模式）；未写入任何内容
	Block
};
