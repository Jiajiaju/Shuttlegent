#include "Chat/FShuttlegentToolPresentation.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "ShuttlegentEditorConstants.h"

#define LOCTEXT_NAMESPACE "ShuttlegentToolPresentation"

namespace FShuttlegentToolPresentationPrivate
{
	// 显示在被截断的摘要或原因之后
	inline const TCHAR* Ellipsis = TEXT("…");

	// 将多行摘要的各行连接为一行
	inline const TCHAR* LineJoiner = TEXT(" ");

	// 生成相对路径时，项目目录之后的目录分隔符
	inline const TCHAR* PathSeparator = TEXT("/");

	FString Truncate(const FString& Text, int32 MaxChars)
	{
		if (Text.Len() <= MaxChars) return Text;

		return Text.Left(MaxChars) + Ellipsis;
	}

	/** 项目下的绝对路径转换为使用正斜杠的项目相对路径。 */
	FString ShortenPath(const FString& Value)
	{
		FString Path = Value;
		FPaths::NormalizeFilename(Path);
		if (FPaths::IsRelative(Path)) return Path;

		FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(ProjectDir);
		if (!Path.StartsWith(ProjectDir, ESearchCase::IgnoreCase)) return Path;

		FString Relative = Path;
		if (!FPaths::MakePathRelativeTo(Relative, *(ProjectDir + PathSeparator))) return Path;
		return Relative;
	}

	bool TryGetValue(const TSharedPtr<FJsonObject>& Input, const TCHAR* Key, FString& OutValue)
	{
		return Input->TryGetStringField(Key, OutValue) && !OutValue.TrimStartAndEnd().IsEmpty();
	}
}

FText FShuttlegentToolPresentation::GetDisplayName(const FString& ToolName)
{
	if (ToolName == ShuttlegentEditorTool::Read) return LOCTEXT("ToolRead", "Read file");
	if (ToolName == ShuttlegentEditorTool::ReadImage) return LOCTEXT("ToolReadImage", "View image");
	if (ToolName == ShuttlegentEditorTool::Write) return LOCTEXT("ToolWrite", "Write file");
	if (ToolName == ShuttlegentEditorTool::Edit) return LOCTEXT("ToolEdit", "Edit file");
	if (ToolName == ShuttlegentEditorTool::Glob) return LOCTEXT("ToolGlob", "Find files");
	if (ToolName == ShuttlegentEditorTool::Grep) return LOCTEXT("ToolGrep", "Search content");
	if (ToolName == ShuttlegentEditorTool::WebSearch) return LOCTEXT("ToolWebSearch", "Web search");
	if (ToolName == ShuttlegentEditorTool::WebFetch) return LOCTEXT("ToolWebFetch", "Read web page");
	if (ToolName == ShuttlegentEditorTool::Subagent) return LOCTEXT("ToolSubagent", "Subtask");
	if (ToolName == ShuttlegentEditorTool::ListSubagentModels) return LOCTEXT("ToolListSubagentModels", "List subtask models");
	if (ToolName == ShuttlegentEditorTool::TodoWrite) return LOCTEXT("ToolTodoWrite", "Update to-dos");
	if (ToolName == ShuttlegentEditorTool::CreateGoal) return LOCTEXT("ToolCreateGoal", "Set goal");
	if (ToolName == ShuttlegentEditorTool::GetGoal) return LOCTEXT("ToolGetGoal", "View goal");
	if (ToolName == ShuttlegentEditorTool::UpdateGoal) return LOCTEXT("ToolUpdateGoal", "Update goal");

	if (ToolName.StartsWith(ShuttlegentEditorTool::McpPrefix))
	{
		FString Server;
		FString Tool;
		const FString Rest = ToolName.RightChop(FCString::Strlen(ShuttlegentEditorTool::McpPrefix));
		if (Rest.Split(ShuttlegentEditorTool::McpSeparator, &Server, &Tool)) return FText::Format(LOCTEXT("ToolMcp", "Editor tool: {0}"), FText::FromString(Tool));
	}
	return FText::FromString(ToolName);
}

bool FShuttlegentToolPresentation::IsReadOnly(const FString& ToolName)
{
	return ToolName == ShuttlegentEditorTool::Read
		|| ToolName == ShuttlegentEditorTool::ReadImage
		|| ToolName == ShuttlegentEditorTool::Glob
		|| ToolName == ShuttlegentEditorTool::Grep
		|| ToolName == ShuttlegentEditorTool::WebSearch
		|| ToolName == ShuttlegentEditorTool::WebFetch
		|| ToolName == ShuttlegentEditorTool::ListSubagentModels
		|| ToolName == ShuttlegentEditorTool::GetGoal;
}

FString FShuttlegentToolPresentation::GetSummary(const FString& InputJson)
{
	if (InputJson.IsEmpty()) return FString();

	TSharedPtr<FJsonObject> Input;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InputJson);
	if (!FJsonSerializer::Deserialize(Reader, Input) || !Input.IsValid()) return FString();

	FString Toolset;
	if (FShuttlegentToolPresentationPrivate::TryGetValue(Input, ShuttlegentEditorTool::ToolsetKey, Toolset))
	{
		FString ShortToolset = Toolset;
		Toolset.Split(ShuttlegentEditorTool::ToolsetNameSeparator, nullptr, &ShortToolset, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		FString Tool;
		const FString Summary = FShuttlegentToolPresentationPrivate::TryGetValue(Input, ShuttlegentEditorTool::ToolKey, Tool) ? FText::Format(LOCTEXT("ToolInToolset", "{0} ({1})"), FText::FromString(Tool), FText::FromString(ShortToolset)).ToString() : ShortToolset;
		return FShuttlegentToolPresentationPrivate::Truncate(Summary, ShuttlegentEditorTool::MaxSummaryChars);
	}

	for (const TCHAR* Key : ShuttlegentEditorTool::SummaryKeys)
	{
		FString Value;
		if (!FShuttlegentToolPresentationPrivate::TryGetValue(Input, Key, Value)) continue;

		const FString KeyName(Key);
		const bool bPath = KeyName == ShuttlegentEditorTool::SearchPathKey || KeyName.EndsWith(ShuttlegentEditorTool::PathKeySuffix);
		FString Summary = bPath ? FShuttlegentToolPresentationPrivate::ShortenPath(Value) : Value;

		// 搜索模式同时注明其搜索位置
		FString SearchPath;
		if (KeyName == ShuttlegentEditorTool::PatternKey && FShuttlegentToolPresentationPrivate::TryGetValue(Input, ShuttlegentEditorTool::SearchPathKey, SearchPath))
		{
			Summary = FText::Format(LOCTEXT("PatternInPath", "{0} ({1})"), FText::FromString(Summary), FText::FromString(FShuttlegentToolPresentationPrivate::ShortenPath(SearchPath))).ToString();
		}

		TArray<FString> Lines;
		Summary.ParseIntoArrayLines(Lines);
		return FShuttlegentToolPresentationPrivate::Truncate(FString::Join(Lines, FShuttlegentToolPresentationPrivate::LineJoiner), ShuttlegentEditorTool::MaxSummaryChars);
	}
	return FString();
}

FString FShuttlegentToolPresentation::GetContentPreview(const FString& InputJson)
{
	if (InputJson.IsEmpty()) return FString();

	TSharedPtr<FJsonObject> Input;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InputJson);
	if (!FJsonSerializer::Deserialize(Reader, Input) || !Input.IsValid()) return FString();

	for (const TCHAR* Key : ShuttlegentEditorTool::ContentKeys)
	{
		FString Value;
		if (!FShuttlegentToolPresentationPrivate::TryGetValue(Input, Key, Value)) continue;

		TArray<FString> Lines;
		Value.TrimStartAndEnd().ParseIntoArrayLines(Lines);
		return FShuttlegentToolPresentationPrivate::Truncate(FString::Join(Lines, FShuttlegentToolPresentationPrivate::LineJoiner), ShuttlegentEditorTool::MaxReasonChars);
	}
	return FString();
}

FString FShuttlegentToolPresentation::GetFailureReason(const FString& Result)
{
	TArray<FString> Lines;
	Result.TrimStartAndEnd().ParseIntoArrayLines(Lines);
	if (Lines.IsEmpty()) return FString();

	FString Line = Lines[0].TrimEnd();
	if (Line.StartsWith(ShuttlegentEditorTool::ErrorPrefix)) Line = Line.RightChop(FCString::Strlen(ShuttlegentEditorTool::ErrorPrefix)).TrimStart();
	return FShuttlegentToolPresentationPrivate::Truncate(Line, ShuttlegentEditorTool::MaxReasonChars);
}

#undef LOCTEXT_NAMESPACE
