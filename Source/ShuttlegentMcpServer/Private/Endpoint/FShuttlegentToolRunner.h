#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "Tool/FShuttlegentToolResult.h"

class FShuttlegentWriteGate;
class IShuttlegentTool;

/**
 * 执行一次工具调用（Shuttlegent.md 第 10.3 节）。只读调用立即答复；对于分派型工具，则在其结果到达时答复。
 * 写入工具先预览，等待写入门，确认后在一个编辑器事务内写入；任一对象失败时整个事务撤销，已写入的对象上报为
 * 已回滚。未在事务中记录任何内容的写入上报为不可撤销。结果交给写入门（面板的结果栏），并以文本形式返回给
 * 模型。输出按输出预算截断。仅限游戏线程。
 */
class FShuttlegentToolRunner
{
public:
	static void Call(const TSharedRef<FShuttlegentWriteGate>& Gate, const TSharedRef<IShuttlegentTool>& Tool, const TSharedRef<FJsonObject>& Arguments, TFunction<void(const FShuttlegentToolResult&)> OnDone);

	/** 将文本截断至输出预算并加以说明；分页工具的首行仍说明截断前发送的范围。 */
	static FString LimitOutput(const FString& Text);
};
