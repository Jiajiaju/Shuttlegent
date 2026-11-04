#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentConnectionStatus.generated.h"

/** 一条模型连接最近一次检测的结论。 */
UENUM()
enum class EShuttlegentConnectionStatus : uint8
{
	// 还没检测过，或地址、密钥改过之后还没重新检测
	Untested,
	// 正在检测
	Testing,
	// 已连通：协议与模型列表已识别
	Connected,
	// 未连通：原因写在 Output Log
	Failed
};
