#pragma once

#include "CoreMinimal.h"

/** 对话视图所在的页签：主页签带会话列表，会话页签只显示一个会话。 */
enum class EShuttlegentViewMode : uint8
{
	// 主页签：会话列表与当前会话
	Hub,

	// 会话页签：从会话列表拖出的一个会话，不显示会话列表
	Session
};
