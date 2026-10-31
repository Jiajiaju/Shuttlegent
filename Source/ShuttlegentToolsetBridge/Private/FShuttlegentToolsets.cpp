#include "FShuttlegentToolsets.h"

#include "Editor.h"
#include "Internationalization/Regex.h"
#include "CoreGlobals.h"
#include "Templates/UnrealTemplate.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetRegistry.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"

#include "FShuttlegentEngineMessageCapture.h"
#include "ShuttlegentProjectSettings.h"
#include "ShuttlegentSettings.h"
#include "ShuttlegentToolsetBridgeConstants.h"

namespace FShuttlegentToolsetsPrivate
{
	UE::ToolsetRegistry::FToolsetRegistry* GetRegistry()
	{
		if (GEditor == nullptr) return nullptr;

		UToolsetRegistrySubsystem* Subsystem = GEditor->GetEditorSubsystem<UToolsetRegistrySubsystem>();
		return IsValid(Subsystem) ? &Subsystem->ToolsetRegistry : nullptr;
	}

	/** 不区分大小写的子串匹配，以斜杠包围时视为正则表达式；与 Toolset Registry 自身的模式语法一致。 */
	bool Matches(const FString& Name, const FString& Pattern)
	{
		const FString Trimmed = Pattern.TrimStartAndEnd();
		if (Trimmed.IsEmpty()) return false;

		const int32 DelimiterLength = FCString::Strlen(ShuttlegentToolsetBridgeConstant::RegexDelimiter);
		if (Trimmed.Len() > 2 * DelimiterLength && Trimmed.StartsWith(ShuttlegentToolsetBridgeConstant::RegexDelimiter) && Trimmed.EndsWith(ShuttlegentToolsetBridgeConstant::RegexDelimiter))
		{
			const FRegexPattern Regex(Trimmed.Mid(DelimiterLength, Trimmed.Len() - 2 * DelimiterLength), ERegexPatternFlags::CaseInsensitive);
			FRegexMatcher Matcher(Regex, Name);
			return Matcher.FindNext();
		}
		return Name.Contains(Trimmed, ESearchCase::IgnoreCase);
	}

	bool IsBlockedByProject(const FString& Name)
	{
		const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
		return IsValid(Project) && Project->BlockedToolsetNames.ContainsByPredicate([&Name](const FString& Pattern) { return Matches(Name, Pattern); });
	}

	bool IsDisabledByUser(const FString& ToolsetName)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		return IsValid(Settings) && Settings->DisabledToolsets.ContainsByPredicate([&ToolsetName](const FString& Disabled) { return Disabled.Equals(ToolsetName, ESearchCase::IgnoreCase); });
	}

	/** 存在以工具自身名称或完整名称指明该工具的条目。 */
	bool IsNamed(const TArray<FString>& Entries, const FShuttlegentToolsetTool& Tool)
	{
		return Entries.ContainsByPredicate([&Tool](const FString& Entry)
		{
			const FString Trimmed = Entry.TrimStartAndEnd();
			return Trimmed.Equals(Tool.Name, ESearchCase::IgnoreCase) || Trimmed.Equals(Tool.FullName, ESearchCase::IgnoreCase);
		});
	}

	/** 名称以完整单词开头：GetActors 与 get_actors 以 Get 开头，Getaway 则不是。 */
	bool StartsWithWord(const FString& ToolName, const FString& Word)
	{
		if (Word.IsEmpty() || !ToolName.StartsWith(Word, ESearchCase::IgnoreCase)) return false;
		if (ToolName.Len() == Word.Len()) return true;

		const TCHAR Next = ToolName[Word.Len()];
		return Next == ShuttlegentToolsetBridgeConstant::WordSeparator || FChar::IsUpper(Next);
	}

	/** 依次检查写入列表、只读列表、只读前缀；其余一律视为写入。 */
	bool IsReadOnly(const FShuttlegentToolsetTool& Tool)
	{
		const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
		if (!IsValid(Project) || IsNamed(Project->WriteTools, Tool)) return false;
		if (IsNamed(Project->ReadOnlyTools, Tool)) return true;

		return Project->ReadOnlyToolPrefixes.ContainsByPredicate([&Tool](const FString& Prefix) { return StartsWithWord(Tool.Name, Prefix.TrimStartAndEnd()); });
	}

	bool IsUndoable(const FShuttlegentToolsetTool& Tool)
	{
		const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
		return !IsValid(Project) || !IsNamed(Project->NonUndoableTools, Tool);
	}

	bool IsDestructive(const FShuttlegentToolsetTool& Tool)
	{
		const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
		return IsValid(Project) && Project->DestructiveToolPrefixes.ContainsByPredicate([&Tool](const FString& Prefix) { return StartsWithWord(Tool.Name, Prefix.TrimStartAndEnd()); });
	}

	/** Toolset Schema 中未被项目屏蔽的工具，按名称排序。 */
	TArray<FShuttlegentToolsetTool> ParseTools(const FString& ToolsetName, const FString& SchemaJson)
	{
		TArray<FShuttlegentToolsetTool> Tools;
		TSharedPtr<FJsonObject> Schema;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SchemaJson);
		if (SchemaJson.IsEmpty() || !FJsonSerializer::Deserialize(Reader, Schema) || !Schema.IsValid()) return Tools;

		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!Schema->TryGetArrayField(ShuttlegentToolsetBridgeConstant::FieldTools, Entries) || Entries == nullptr) return Tools;

		const FString Qualifier = ToolsetName + ShuttlegentToolsetBridgeConstant::NameSeparator;
		for (const TSharedPtr<FJsonValue>& Entry : *Entries)
		{
			const TSharedPtr<FJsonObject>* EntryObject = nullptr;
			if (!Entry.IsValid() || !Entry->TryGetObject(EntryObject) || EntryObject == nullptr || !EntryObject->IsValid()) continue;

			FShuttlegentToolsetTool Tool;
			if (!(*EntryObject)->TryGetStringField(ShuttlegentToolsetBridgeConstant::FieldName, Tool.FullName) || Tool.FullName.IsEmpty()) continue;

			// 完整名称形如 Toolset.Tool，而 Toolset 名称本身也包含点号
			if (Tool.FullName.StartsWith(Qualifier)) Tool.Name = Tool.FullName.RightChop(Qualifier.Len());
			else if (!Tool.FullName.Split(ShuttlegentToolsetBridgeConstant::NameSeparator, nullptr, &Tool.Name, ESearchCase::CaseSensitive, ESearchDir::FromEnd)) Tool.Name = Tool.FullName;
			if (Tool.Name.IsEmpty() || IsBlockedByProject(Tool.FullName)) continue;

			(*EntryObject)->TryGetStringField(ShuttlegentToolsetBridgeConstant::FieldDescription, Tool.Description);
			const TSharedPtr<FJsonObject>* InputSchema = nullptr;
			if ((*EntryObject)->TryGetObjectField(ShuttlegentToolsetBridgeConstant::FieldInputSchema, InputSchema) && InputSchema != nullptr) Tool.InputSchema = *InputSchema;
			Tool.bReadOnly = IsReadOnly(Tool);
			Tool.bUndoable = IsUndoable(Tool);
			Tool.bDestructive = IsDestructive(Tool);
			Tools.Add(MoveTemp(Tool));
		}

		Tools.Sort([](const FShuttlegentToolsetTool& Left, const FShuttlegentToolsetTool& Right) { return Left.Name < Right.Name; });
		return Tools;
	}

	/** 返回已注册的 Toolset，前提是注册表已启用它、项目未屏蔽它且用户未将其关闭。 */
	TSharedPtr<UE::ToolsetRegistry::FToolset> FindUsable(const FString& ToolsetName, FString& OutError)
	{
		UE::ToolsetRegistry::FToolsetRegistry* Registry = GetRegistry();
		if (Registry == nullptr)
		{
			OutError = TEXT("The engine Toolset Registry is not available in this editor.");
			return nullptr;
		}

		TSharedPtr<UE::ToolsetRegistry::FToolset> Toolset = Registry->Find(ToolsetName);
		if (!Toolset.IsValid() || !Toolset->IsEnabled())
		{
			OutError = FString::Printf(TEXT("Unknown toolset %s. Call list_toolsets for the toolsets you can use."), *ToolsetName);
			return nullptr;
		}
		if (IsBlockedByProject(ToolsetName))
		{
			OutError = FString::Printf(TEXT("Toolset %s is blocked in the project's Shuttlegent settings."), *ToolsetName);
			return nullptr;
		}
		if (IsDisabledByUser(ToolsetName))
		{
			OutError = FString::Printf(TEXT("The user switched toolset %s off in the Shuttlegent settings."), *ToolsetName);
			return nullptr;
		}
		return Toolset;
	}
}

bool FShuttlegentToolsets::IsAvailable()
{
	return FShuttlegentToolsetsPrivate::GetRegistry() != nullptr;
}

TArray<FShuttlegentToolsetInfo> FShuttlegentToolsets::GetToolsets()
{
	TArray<FShuttlegentToolsetInfo> Toolsets;
	const UE::ToolsetRegistry::FToolsetRegistry* Registry = FShuttlegentToolsetsPrivate::GetRegistry();
	if (Registry == nullptr) return Toolsets;

	Registry->ForEachToolset([&Toolsets](const FString& Name, const UE::ToolsetRegistry::FToolset& Toolset)
	{
		FShuttlegentToolsetInfo& Info = Toolsets.AddDefaulted_GetRef();
		Info.Name = Name;
		Info.Description = Toolset.GetToolsetDescription();
		Info.bBlockedByProject = FShuttlegentToolsetsPrivate::IsBlockedByProject(Name);
		Info.bDisabledByUser = FShuttlegentToolsetsPrivate::IsDisabledByUser(Name);
		if (!Info.bBlockedByProject && !Info.bDisabledByUser) Info.ToolCount = FShuttlegentToolsetsPrivate::ParseTools(Name, Toolset.GetJsonSchema()).Num();
	});

	Toolsets.Sort([](const FShuttlegentToolsetInfo& Left, const FShuttlegentToolsetInfo& Right) { return Left.Name < Right.Name; });
	return Toolsets;
}

bool FShuttlegentToolsets::GetTools(const FString& ToolsetName, TArray<FShuttlegentToolsetTool>& OutTools, FString& OutError)
{
	const TSharedPtr<UE::ToolsetRegistry::FToolset> Toolset = FShuttlegentToolsetsPrivate::FindUsable(ToolsetName, OutError);
	if (!Toolset.IsValid()) return false;

	OutTools = FShuttlegentToolsetsPrivate::ParseTools(ToolsetName, Toolset->GetJsonSchema());
	if (!OutTools.IsEmpty()) return true;

	OutError = FString::Printf(TEXT("Toolset %s has no tools you can use."), *ToolsetName);
	return false;
}

bool FShuttlegentToolsets::ReturnedFalse(const FString& ResultText)
{
	// JSON 之后可能附有引擎消息；只有其前面的部分才是工具的应答
	const int32 NoteStart = ResultText.Find(ShuttlegentToolsetBridgeConstant::EngineMessagesHeading, ESearchCase::CaseSensitive);
	const FString Answer = NoteStart == INDEX_NONE ? ResultText : ResultText.Left(NoteStart);

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Answer);
	TSharedPtr<FJsonObject> Result;
	if (!FJsonSerializer::Deserialize(Reader, Result) || !Result.IsValid()) return false;

	bool bValue = true;
	return Result->TryGetBoolField(ShuttlegentToolsetBridgeConstant::FieldReturnValue, bValue) && !bValue;
}

bool FShuttlegentToolsets::FindTool(const FString& ToolsetName, const FString& ToolName, FShuttlegentToolsetTool& OutTool, FString& OutError)
{
	TArray<FShuttlegentToolsetTool> Tools;
	if (!GetTools(ToolsetName, Tools, OutError)) return false;

	const FShuttlegentToolsetTool* Found = Tools.FindByPredicate([&ToolName](const FShuttlegentToolsetTool& Tool) { return Tool.Name.Equals(ToolName, ESearchCase::IgnoreCase) || Tool.FullName.Equals(ToolName, ESearchCase::IgnoreCase); });
	if (Found == nullptr)
	{
		OutError = FString::Printf(TEXT("Toolset %s has no tool %s you can use. Call describe_toolset to see its tools."), *ToolsetName, *ToolName);
		return false;
	}

	OutTool = *Found;
	return true;
}

TFuture<TValueOrError<FString, FString>> FShuttlegentToolsets::Execute(const FString& ToolsetName, const FString& ToolName, const FString& JsonArguments)
{
	UE::ToolsetRegistry::FToolsetRegistry* Registry = FShuttlegentToolsetsPrivate::GetRegistry();
	if (Registry == nullptr) return MakeFulfilledPromise<TValueOrError<FString, FString>>(MakeError(FString(TEXT("The engine Toolset Registry is not available in this editor.")))).GetFuture();

	// 引擎消息对话框会阻塞编辑器直到有人响应：以无人值守方式运行工具，使对话框只写日志并采用默认答复，
	// 并将引擎报告的内容随结果一并交给助手
	FShuttlegentEngineMessageCapture Capture;
	TFuture<TValueOrError<FString, FString>> Future;
	{
		TGuardValue<bool> Unattended(GIsRunningUnattendedScript, true);
		Future = Registry->ExecuteTool(UE::ToolsetRegistry::FToolDescriptor { ToolsetName, ToolName }, JsonArguments);
	}

	const FString Messages = Capture.GetMessages();
	const FString Note = Messages.IsEmpty() ? FString() : ShuttlegentToolsetBridgeConstant::EngineMessagesHeading + Messages;
	return Future.Next([Note](TValueOrError<FString, FString> Result) -> TValueOrError<FString, FString>
	{
		if (Result.HasError()) return MakeError(Result.GetError() + Note);
		return MakeValue(Result.GetValue() + Note);
	});
}
