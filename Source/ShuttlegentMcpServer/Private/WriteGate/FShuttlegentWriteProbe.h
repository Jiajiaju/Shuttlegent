#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

class FShuttlegentWriteGate;

/**
 * Shuttlegent.WriteProbe：一个最小的同步写入工具，与 MCP 端点的写入工具完全一样经由 FShuttlegentWriteGate
 * （Shuttlegent.md 第 14.3.6 节）。它请求将选中的 Actor 上移，确认后在一个编辑器事务内移动它们，并上报每个 Actor
 * 的真实结果。可选的索引会令该 Actor 失败并使其余 Actor 不执行，用于演示部分完成的情形。
 */
class FShuttlegentWriteProbe
{
private:
	TWeakPtr<FShuttlegentWriteGate> Gate;
	TUniquePtr<FAutoConsoleCommand> Command;

public:
	explicit FShuttlegentWriteProbe(const TSharedRef<FShuttlegentWriteGate>& InGate);

private:
	void HandleCommand(const TArray<FString>& Args);
};
