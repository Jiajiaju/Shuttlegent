#pragma once

#include "CoreMinimal.h"

/** 聊天面板中一行的类型。 */
enum class EShuttlegentChatEntryKind : uint8
{
	// 用户发送的提示
	User,

	// 某一消息 id 的助手回复文本（及其折叠的推理内容）
	Assistant,

	// 一次工具调用及其最新状态
	ToolCall,

	// 运行时的权限请求，在面板中作答
	Permission,

	// 未正常结束的回合（已取消或已失败）
	TurnEnd,

	// 连接与会话通知
	Notice,

	// 一次已批准的编辑器写入逐个对象的执行结果，及其撤销
	WriteResult,

	// 一次已批准的编辑器写入因运行时丢失而未收到结果；由用户核对其对象
	WriteUnknown
};
