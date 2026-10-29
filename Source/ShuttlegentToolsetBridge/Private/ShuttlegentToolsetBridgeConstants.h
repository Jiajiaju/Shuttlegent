#pragma once

#include "CoreMinimal.h"

/** Engine toolsets of the Toolset Registry as Shuttlegent editor tools. */
namespace ShuttlegentToolsetBridgeConstant
{
	// Tool names
	inline const TCHAR* ListToolsetsName = TEXT("list_toolsets");
	inline const TCHAR* DescribeToolsetName = TEXT("describe_toolset");
	inline const TCHAR* CallToolsetToolName = TEXT("call_toolset_tool");

	// Argument names
	inline const TCHAR* ArgToolset = TEXT("toolset");
	inline const TCHAR* ArgTool = TEXT("tool");
	inline const TCHAR* ArgArguments = TEXT("arguments");

	// Fields of a toolset's JSON schema
	inline const TCHAR* FieldTools = TEXT("tools");
	inline const TCHAR* FieldName = TEXT("name");
	inline const TCHAR* FieldDescription = TEXT("description");
	inline const TCHAR* FieldInputSchema = TEXT("inputSchema");

	// Separates toolset and tool in a full tool name (Toolset.Tool), and the parts of a toolset name
	inline const TCHAR* NameSeparator = TEXT(".");

	// A pattern enclosed in these is a regular expression
	inline const TCHAR* RegexDelimiter = TEXT("/");

	// Separates a tool name's leading word from the rest in snake_case names
	inline constexpr TCHAR WordSeparator = TEXT('_');

	// End of the first line of a description
	inline const TCHAR* LineBreak = TEXT("\n");

	// Longest description line and argument summary shown, in characters; longer ones end with an ellipsis
	inline constexpr int32 MaxSummaryChars = 160;
	inline constexpr int32 MaxArgumentChars = 400;
	inline const TCHAR* Ellipsis = TEXT("…");

	// Arguments passed to a toolset tool that takes none
	inline const TCHAR* EmptyArguments = TEXT("{}");
}
