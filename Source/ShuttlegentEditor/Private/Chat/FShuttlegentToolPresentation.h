#pragma once

#include "CoreMinimal.h"

/** 运行时工具调用在聊天行上的呈现方式：可读名称、单行对象摘要、失败原因。均为纯函数。 */
struct FShuttlegentToolPresentation
{
	/** 工具的可读名称（"read" -> "读取文件"）；MCP 工具显示其工具部分；未知名称原样显示。 */
	static FText GetDisplayName(const FString& ToolName);

	/** 该工具的连续调用是否可折叠为“已查询 N 项”分组。 */
	static bool IsReadOnly(const FString& ToolName);

	/** 由 rawInput JSON 生成的单行对象摘要：取第一个已知键，路径相对于项目，并截断。 */
	static FString GetSummary(const FString& InputJson);

	/** 调用所写入或执行的内容，取自 rawInput JSON（content、new_string 或 command），合并为一行并截断；不存在时为空。 */
	static FString GetContentPreview(const FString& InputJson);

	/** 失败调用结果的第一行，去掉运行时的 "Error:" 前缀并截断。 */
	static FString GetFailureReason(const FString& Result);
};
