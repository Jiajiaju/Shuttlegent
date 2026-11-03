#pragma once

#include "CoreMinimal.h"

/**
 * 端点分发一次 tools/call 期间所属的对话（请求头 X-Shuttlegent-Conversation 的值）。工具在 FShuttlegentToolRunner::Call
 * 中同步进入写入门与提问通道，它们在进入时取出该值记到请求上，对话模型据此把确认卡、问题与选择卡放进发起调用的对话。
 * 作用域可嵌套，结束时恢复外层的值；作用域之外为空。仅限游戏线程。
 */
class FShuttlegentToolCallScope
{
private:
	FString PreviousKey;

	/** 当前作用域的对话标识。 */
	static FString CurrentKey;

public:
	explicit FShuttlegentToolCallScope(const FString& ConversationKey);
	~FShuttlegentToolCallScope();

	FShuttlegentToolCallScope(const FShuttlegentToolCallScope&) = delete;
	FShuttlegentToolCallScope& operator=(const FShuttlegentToolCallScope&) = delete;

	/** 正在执行的工具调用所属的对话；没有正在执行的调用或调用未带请求头时为空。 */
	static const FString& GetConversationKey() { return CurrentKey; }
};
