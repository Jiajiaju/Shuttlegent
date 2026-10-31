#pragma once

#include "CoreMinimal.h"

/** 运行时无法处理提问的原因，由面板的启动引导卡展示（Shuttlegent.md 第 14.3.8 节）。 */
enum class EShuttlegentStartIssue : uint8
{
	// 没有已知问题
	None,

	// 运行时可执行文件或其配置补丁不在插件预期的位置
	MissingRuntime,

	// 个人设置中没有默认模型供应商的密钥
	MissingKey,

	// 供应商拒绝了密钥
	AuthFailed,

	// 运行时因其他原因启动或初始化失败
	Other
};
