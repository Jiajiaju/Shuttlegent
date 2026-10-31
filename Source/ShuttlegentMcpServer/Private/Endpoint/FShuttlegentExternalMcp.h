#pragma once

#include "CoreMinimal.h"

class FJsonValue;
struct FShuttlegentExternalMcpState;

/**
 * 项目外部 MCP 服务器的 ACP mcpServers 条目（Shuttlegent.md 第 11 节）。任一已声明的服务器启动失败时，运行时会
 * 拒绝会话，因此无法工作的条目会被剔除并给出警告：缺少名称、使用编辑器端点的名称、http 条目缺少 URL、stdio
 * 命令不是绝对路径，或请求头引用的变量没有值。请求头的值按 provider key 的方式解析，且从不写入日志。
 */
struct FShuttlegentExternalMcp
{
	static TArray<TSharedPtr<FJsonValue>> MakeDeclarations();

	/** 按顺序列出项目设置中的全部服务器及其被剔除的原因；供面板使用，不写日志。 */
	static TArray<FShuttlegentExternalMcpState> DescribeServers();
};
