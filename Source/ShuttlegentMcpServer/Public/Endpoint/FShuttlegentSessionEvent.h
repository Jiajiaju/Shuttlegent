#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * session tap 运行时插件推来的一条会话事件，即 ACP 不转发的那些：待办（todo/write）、计划模式（plan/mode）、
 * 压缩（compaction/*）与目标（goal/change）。Data 只含面板需要的字段，各类型的字段见插件
 * Runtime/plugins/shuttlegent-session-tap.mjs。
 */
struct FShuttlegentSessionEvent
{
	/** 事件所属的 ACP 会话。 */
	FString SessionId;

	/** 运行时的事件类型，如 todo/write。 */
	FString Type;

	TSharedPtr<FJsonObject> Data;
};
