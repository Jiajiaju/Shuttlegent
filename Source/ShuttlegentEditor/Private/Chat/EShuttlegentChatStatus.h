#pragma once

#include "CoreMinimal.h"

/** 面板标题栏为当前对话报告的状态（Shuttlegent UI 交互规范第 3 节）。 */
enum class EShuttlegentChatStatus : uint8
{
	// 尚无运行时进程
	NotConnected,

	// 运行时已启动，等待 initialize
	Connecting,

	// 运行时就绪且空闲；上一回合（如有）已完成
	Connected,

	// 回合正在运行
	Querying,

	// 回合正在等待用户答复运行时的权限请求
	AwaitingPermission,

	// 编辑器写入正在等待用户确认
	AwaitingWriteConfirmation,

	// 已确认的编辑器写入正在执行
	Writing,

	// 已发送停止；等待回合结束
	Stopping,

	// 上一回合被用户或运行时停止
	Stopped,

	// 上一回合以错误结束
	Failed,

	// 运行时意外退出
	Lost,

	// 正在运行时中恢复已存储的会话
	Resuming,

	// 运行时无法启动或无法处理提示（缺少运行时或密钥、密钥被拒绝）；由启动引导卡片说明原因
	StartBlocked,

	// 运行时无法恢复会话上下文；其本地记录予以保留
	ResumeFailed
};
