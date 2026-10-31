#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "Tool/FShuttlegentToolDefinition.h"
#include "Tool/FShuttlegentToolResult.h"
#include "WriteGate/FShuttlegentWriteObject.h"

struct FShuttlegentWriteRequest;

/**
 * MCP 端点提供的单个编辑器工具。在游戏线程上运行。
 *
 * 只读工具实现 Run。写入工具改为实现 Preview 与 Execute：Preview 描述将发生的变更（即确认卡），端点随后请求
 * 写入门，仅在确认后由 Execute 写入预览中的对象。事务性写入在端点开启的一个编辑器事务内运行；Execute 必须对
 * 其修改的对象调用 Modify()，且不得自行开启事务。
 *
 * 分派给其他工具的工具（引擎 Toolset 调用）按每次调用判断是否仅读取（IsReadOnlyCall），并可延后答复读取
 * 请求（RunAsync）。
 */
class SHUTTLEGENTMCPSERVER_API IShuttlegentTool
{
public:
	virtual ~IShuttlegentTool() = default;

	virtual const FShuttlegentToolDefinition& GetDefinition() const = 0;

	/** 本次调用是否仅读取，从而无需确认即可运行；默认取定义中的 bReadOnly。 */
	virtual bool IsReadOnlyCall(const TSharedRef<FJsonObject>& Arguments) const;

	/** 只读调用：通过 OnDone 答复，可立即答复，也可稍后在游戏线程上答复；默认调用 Run。 */
	virtual void RunAsync(const TSharedRef<FJsonObject>& Arguments, TFunction<void(const FShuttlegentToolResult&)> OnDone);

	/** 只读工具：答复本次调用。 */
	virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments);

	/**
	 * 写入工具：填写 OutRequest 的 Action、Level 与 Objects（名称、路径、字段、当前值与计划值），不作任何修改。
	 * 返回 false 并给出 OutError，即在征询任何人之前拒绝本次调用。
	 */
	virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError);

	/** 写入工具，确认后调用：按顺序写入预览中的对象，为每个对象设置 Status，未完成的对象还须设置 Message。 */
	virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects);
};
