#include "Chat/FShuttlegentToolPresentation.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "ShuttlegentEditorConstants.h"

#define LOCTEXT_NAMESPACE "ShuttlegentToolPresentation"

namespace FShuttlegentToolPresentationPrivate
{
	// Shown after a truncated summary or reason
	inline const TCHAR* Ellipsis = TEXT("…");

	// Joins the lines of a multi-line summary into one row
	inline const TCHAR* LineJoiner = TEXT(" ");

	// Directory separator after the project directory when making paths relative
	inline const TCHAR* PathSeparator = TEXT("/");

	FString Truncate(const FString& Text, int32 MaxChars)
	{
		if (Text.Len() <= MaxChars) return Text;

		return Text.Left(MaxChars) + Ellipsis;
	}

	/** Absolute paths under the project become project-relative with forward slashes. */
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
	if (ToolName == ShuttlegentEditorTool::Read) return LOCTEXT("ToolRead", "读取文件");
	if (ToolName == ShuttlegentEditorTool::ReadImage) return LOCTEXT("ToolReadImage", "查看图片");
	if (ToolName == ShuttlegentEditorTool::Write) return LOCTEXT("ToolWrite", "写入文件");
	if (ToolName == ShuttlegentEditorTool::Edit) return LOCTEXT("ToolEdit", "编辑文件");
	if (ToolName == ShuttlegentEditorTool::Glob) return LOCTEXT("ToolGlob", "查找文件");
	if (ToolName == ShuttlegentEditorTool::Grep) return LOCTEXT("ToolGrep", "搜索内容");
	if (ToolName == ShuttlegentEditorTool::WebSearch) return LOCTEXT("ToolWebSearch", "网页搜索");
	if (ToolName == ShuttlegentEditorTool::WebFetch) return LOCTEXT("ToolWebFetch", "读取网页");
	if (ToolName == ShuttlegentEditorTool::Subagent) return LOCTEXT("ToolSubagent", "子任务");
	if (ToolName == ShuttlegentEditorTool::ListSubagentModels) return LOCTEXT("ToolListSubagentModels", "查看子任务模型");
	if (ToolName == ShuttlegentEditorTool::TodoWrite) return LOCTEXT("ToolTodoWrite", "更新待办");
	if (ToolName == ShuttlegentEditorTool::CreateGoal) return LOCTEXT("ToolCreateGoal", "设定目标");
	if (ToolName == ShuttlegentEditorTool::GetGoal) return LOCTEXT("ToolGetGoal", "查看目标");
	if (ToolName == ShuttlegentEditorTool::UpdateGoal) return LOCTEXT("ToolUpdateGoal", "更新目标");

	if (ToolName.StartsWith(ShuttlegentEditorTool::McpPrefix))
	{
		FString Server;
		FString Tool;
		const FString Rest = ToolName.RightChop(FCString::Strlen(ShuttlegentEditorTool::McpPrefix));
		if (Rest.Split(ShuttlegentEditorTool::McpSeparator, &Server, &Tool)) return FText::Format(LOCTEXT("ToolMcp", "编辑器工具：{0}"), FText::FromString(Tool));
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

	for (const TCHAR* Key : ShuttlegentEditorTool::SummaryKeys)
	{
		FString Value;
		if (!FShuttlegentToolPresentationPrivate::TryGetValue(Input, Key, Value)) continue;

		const FString KeyName(Key);
		const bool bPath = KeyName == ShuttlegentEditorTool::SearchPathKey || KeyName.EndsWith(ShuttlegentEditorTool::PathKeySuffix);
		FString Summary = bPath ? FShuttlegentToolPresentationPrivate::ShortenPath(Value) : Value;

		// A search pattern also says where it looked
		FString SearchPath;
		if (KeyName == ShuttlegentEditorTool::PatternKey && FShuttlegentToolPresentationPrivate::TryGetValue(Input, ShuttlegentEditorTool::SearchPathKey, SearchPath))
		{
			Summary = FText::Format(LOCTEXT("PatternInPath", "{0}（{1}）"), FText::FromString(Summary), FText::FromString(FShuttlegentToolPresentationPrivate::ShortenPath(SearchPath))).ToString();
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
