#pragma once

#include "CoreMinimal.h"

/** 将 Toolset Registry 中的引擎 Toolset 作为 Shuttlegent 编辑器工具提供。 */
namespace ShuttlegentToolsetBridgeConstant
{
	// 工具名
	inline const TCHAR* ListToolsetsName = TEXT("list_toolsets");
	inline const TCHAR* DescribeToolsetName = TEXT("describe_toolset");
	inline const TCHAR* CallToolsetToolName = TEXT("call_toolset_tool");

	// 参数名
	inline const TCHAR* ArgToolset = TEXT("toolset");
	inline const TCHAR* ArgTool = TEXT("tool");
	inline const TCHAR* ArgArguments = TEXT("arguments");

	// Toolset JSON Schema 中的字段
	inline const TCHAR* FieldTools = TEXT("tools");
	inline const TCHAR* FieldName = TEXT("name");
	inline const TCHAR* FieldDescription = TEXT("description");
	inline const TCHAR* FieldInputSchema = TEXT("inputSchema");

	// Toolset 工具结果中存放工具返回值的字段
	inline const TCHAR* FieldReturnValue = TEXT("returnValue");

	// 工具返回 false 时的错误信息，其后接原始结果
	inline const TCHAR* ReturnedFalseError = TEXT("The tool returned false: it did not do what was asked. Nothing may have changed. Result: ");

	// 分隔完整工具名（Toolset.Tool）中的 Toolset 与工具，以及 Toolset 名称的各部分
	inline const TCHAR* NameSeparator = TEXT(".");

	// 以此符号包围的模式视为正则表达式
	inline const TCHAR* RegexDelimiter = TEXT("/");

	// 在 snake_case 名称中分隔工具名的首个单词与其余部分
	inline constexpr TCHAR WordSeparator = TEXT('_');

	// 描述首行的结束符
	inline const TCHAR* LineBreak = TEXT("\n");

	// 显示的描述行与参数摘要的最大长度（字符数）；超出部分以省略号结尾
	inline constexpr int32 MaxSummaryChars = 160;
	inline constexpr int32 MaxArgumentChars = 400;
	inline const TCHAR* Ellipsis = TEXT("…");

	// 传给无参数 Toolset 工具的参数
	inline const TCHAR* EmptyArguments = TEXT("{}");

	// 对话框仅写日志时，承载消息对话框文本的日志行前缀（FMessageDialog::Open，MessageDialog.cpp）
	inline const TCHAR* MessageDialogPrefix = TEXT("Message dialog");

	// 随单次工具结果交给助手的引擎消息上限，以及这些消息所在的标题
	inline constexpr int32 MaxEngineMessages = 20;
	inline const TCHAR* EngineMessagesHeading = TEXT("\n\nEngine messages while the tool ran (dialogs were not shown):\n");
}
