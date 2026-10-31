#include "FShuttlegentToolsetTools.h"

#include "Async/Async.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "FShuttlegentToolsets.h"
#include "ShuttlegentToolsetBridgeConstants.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"
#include "Tool/IShuttlegentTool.h"
#include "WriteGate/FShuttlegentWriteRequest.h"

#define LOCTEXT_NAMESPACE "ShuttlegentToolsetTools"

namespace FShuttlegentToolsetToolsPrivate
{
	FString Truncate(const FString& Text, int32 MaxChars)
	{
		if (Text.Len() <= MaxChars) return Text;

		return Text.Left(MaxChars) + ShuttlegentToolsetBridgeConstant::Ellipsis;
	}

	FString FirstLine(const FString& Text)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		FString Line = Trimmed;
		Trimmed.Split(ShuttlegentToolsetBridgeConstant::LineBreak, &Line, nullptr);
		return Truncate(Line.TrimEnd(), ShuttlegentToolsetBridgeConstant::MaxSummaryChars);
	}

	FString CompactJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Json;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
		FJsonSerializer::Serialize(Object, Writer);
		return Json;
	}

	/** 确认卡片上显示的 Toolset 名称末段：EditorToolset.EditorActorTools -> EditorActorTools。 */
	FString ShortName(const FString& ToolsetName)
	{
		FString Short = ToolsetName;
		ToolsetName.Split(ShuttlegentToolsetBridgeConstant::NameSeparator, nullptr, &Short, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		return Short;
	}

	/** 转交给 Toolset 工具的参数对象；模型以 JSON 字符串形式发送时同样能够识别。 */
	FString GetToolArguments(const TSharedRef<FJsonObject>& Arguments)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (Arguments->TryGetObjectField(ShuttlegentToolsetBridgeConstant::ArgArguments, Object) && Object != nullptr && Object->IsValid()) return CompactJson(Object->ToSharedRef());

		FString Text;
		TSharedPtr<FJsonObject> Parsed;
		if (Arguments->TryGetStringField(ShuttlegentToolsetBridgeConstant::ArgArguments, Text) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Parsed) && Parsed.IsValid()) return CompactJson(Parsed.ToSharedRef());

		return ShuttlegentToolsetBridgeConstant::EmptyArguments;
	}

	bool FindCalledTool(const TSharedRef<FJsonObject>& Arguments, FShuttlegentToolsetTool& OutTool, FString& OutToolset, FString& OutError)
	{
		OutToolset = FShuttlegentToolArguments::GetString(Arguments, ShuttlegentToolsetBridgeConstant::ArgToolset).TrimStartAndEnd();
		const FString ToolName = FShuttlegentToolArguments::GetString(Arguments, ShuttlegentToolsetBridgeConstant::ArgTool).TrimStartAndEnd();
		if (OutToolset.IsEmpty() || ToolName.IsEmpty())
		{
			OutError = TEXT("Pass both toolset and tool, as list_toolsets and describe_toolset name them.");
			return false;
		}
		return FShuttlegentToolsets::FindTool(OutToolset, ToolName, OutTool, OutError);
	}

	/** 保存各工具在构造函数中填写的定义。 */
	class FToolsetToolBase : public IShuttlegentTool
	{
	protected:
		FShuttlegentToolDefinition Definition {};

	public:
		//~ Begin IShuttlegentTool Interface.
		virtual const FShuttlegentToolDefinition& GetDefinition() const override { return Definition; }
		//~ End IShuttlegentTool Interface.
	};

	class FListToolsetsTool : public FToolsetToolBase
	{
	public:
		FListToolsetsTool()
		{
			Definition.Name = ShuttlegentToolsetBridgeConstant::ListToolsetsName;
			Definition.Description = TEXT("List the Unreal Engine toolsets enabled in this editor (UMG, Niagara, PCG, Gameplay Tags, StateTree and more), with what each is for. They hold many more editor tools than this server's own. Use describe_toolset to see a toolset's tools, then call_toolset_tool. Prefer this server's own tools when one fits.");
			Definition.InputSchema = FShuttlegentToolSchema().Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			if (!FShuttlegentToolsets::IsAvailable()) return FShuttlegentToolResult::Error(TEXT("The engine Toolset Registry is not available in this editor."));

			TArray<FString> Lines;
			for (const FShuttlegentToolsetInfo& Toolset : FShuttlegentToolsets::GetToolsets())
			{
				if (Toolset.ToolCount == 0) continue;

				Lines.Add(FString::Printf(TEXT("- %s (%d tools): %s"), *Toolset.Name, Toolset.ToolCount, *FirstLine(Toolset.Description)));
			}
			if (Lines.IsEmpty()) return FShuttlegentToolResult::Ok(TEXT("No engine toolset is usable. Engine toolsets are separate editor plugins (Edit > Plugins, search for Toolset) and are off by default; the project or the user may also have switched some off in the Shuttlegent settings."));

			return FShuttlegentToolResult::Ok(FString::Printf(TEXT("%d engine toolsets. describe_toolset shows a toolset's tools; call_toolset_tool runs one.\n"), Lines.Num()) + FString::Join(Lines, TEXT("\n")));
		}
	};

	class FDescribeToolsetTool : public FToolsetToolBase
	{
	public:
		FDescribeToolsetTool()
		{
			Definition.Name = ShuttlegentToolsetBridgeConstant::DescribeToolsetName;
			Definition.Description = TEXT("Show an engine toolset's tools, each marked read (runs at once) or write (the user confirms first). Pass tool to see that tool's full description and its arguments schema before calling it.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ShuttlegentToolsetBridgeConstant::ArgToolset, TEXT("Toolset name exactly as list_toolsets gives it."), true)
				.String(ShuttlegentToolsetBridgeConstant::ArgTool, TEXT("One tool's name, to see its description and arguments; leave out to list the toolset's tools."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const FString ToolsetName = FShuttlegentToolArguments::GetString(Arguments, ShuttlegentToolsetBridgeConstant::ArgToolset).TrimStartAndEnd();
			const FString ToolName = FShuttlegentToolArguments::GetString(Arguments, ShuttlegentToolsetBridgeConstant::ArgTool).TrimStartAndEnd();
			FString Error;
			if (!ToolName.IsEmpty())
			{
				FShuttlegentToolsetTool Tool;
				if (!FShuttlegentToolsets::FindTool(ToolsetName, ToolName, Tool, Error)) return FShuttlegentToolResult::Error(Error);

				const FString Kind = Tool.bReadOnly ? TEXT("Read: runs at once.") : TEXT("Write: the user confirms each call first.");
				const FString Schema = Tool.InputSchema.IsValid() ? CompactJson(Tool.InputSchema.ToSharedRef()) : FString(ShuttlegentToolsetBridgeConstant::EmptyArguments);
				return FShuttlegentToolResult::Ok(FString::Printf(TEXT("%s\n%s\n\n%s\n\nArguments (JSON Schema), passed to call_toolset_tool as arguments:\n%s"), *Tool.FullName, *Kind, *Tool.Description.TrimStartAndEnd(), *Schema));
			}

			TArray<FShuttlegentToolsetTool> Tools;
			if (!FShuttlegentToolsets::GetTools(ToolsetName, Tools, Error)) return FShuttlegentToolResult::Error(Error);

			TArray<FString> Lines;
			for (const FShuttlegentToolsetTool& Tool : Tools)
			{
				Lines.Add(FString::Printf(TEXT("- %s [%s]: %s"), *Tool.Name, Tool.bReadOnly ? TEXT("read") : TEXT("write"), *FirstLine(Tool.Description)));
			}
			return FShuttlegentToolResult::Ok(FString::Printf(TEXT("%s: %d tools. Pass tool to see one tool's arguments.\n"), *ToolsetName, Tools.Num()) + FString::Join(Lines, TEXT("\n")));
		}
	};

	/**
	 * 执行单个引擎 Toolset 工具。只读工具在结果返回时即应答。其他工具会在确认卡片上显示其参数，
	 * 并在端点的事务中执行；仅当引擎记录了相应操作时才可撤销。
	 */
	class FCallToolsetTool : public FToolsetToolBase
	{
	public:
		FCallToolsetTool()
		{
			Definition.Name = ShuttlegentToolsetBridgeConstant::CallToolsetToolName;
			Definition.Description = TEXT("Run one engine toolset tool. Read tools answer at once; every other tool asks the user to confirm first (55 s limit) and runs as one step, undoable where the engine records it. Check the tool's arguments with describe_toolset first.");
			Definition.bReadOnly = false;
			Definition.bTransactional = true;
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ShuttlegentToolsetBridgeConstant::ArgToolset, TEXT("Toolset name exactly as list_toolsets gives it."), true)
				.String(ShuttlegentToolsetBridgeConstant::ArgTool, TEXT("Tool name as describe_toolset gives it."), true)
				.Object(ShuttlegentToolsetBridgeConstant::ArgArguments, TEXT("The tool's arguments as its schema in describe_toolset describes them; leave out when it takes none."))
				.Build();
		}

		virtual bool IsReadOnlyCall(const TSharedRef<FJsonObject>& Arguments) const override
		{
			FShuttlegentToolsetTool Tool;
			FString Toolset;
			FString Error;
			return FindCalledTool(Arguments, Tool, Toolset, Error) && Tool.bReadOnly;
		}

		virtual void RunAsync(const TSharedRef<FJsonObject>& Arguments, TFunction<void(const FShuttlegentToolResult&)> OnDone) override
		{
			FShuttlegentToolsetTool Tool;
			FString Toolset;
			FString Error;
			if (!FindCalledTool(Arguments, Tool, Toolset, Error))
			{
				OnDone(FShuttlegentToolResult::Error(Error));
				return;
			}

			FShuttlegentToolsets::Execute(Toolset, Tool.Name, GetToolArguments(Arguments)).Then([OnDone](TFuture<TValueOrError<FString, FString>> Future)
			{
				const TValueOrError<FString, FString> Result = Future.Get();
				const FShuttlegentToolResult Answer = Result.HasError() ? FShuttlegentToolResult::Error(Result.GetError()) : FShuttlegentToolResult::Ok(Result.GetValue());
				if (IsInGameThread())
				{
					OnDone(Answer);
					return;
				}

				// 端点在游戏线程上应答
				AsyncTask(ENamedThreads::GameThread, [OnDone, Answer]() { OnDone(Answer); });
			});
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			FShuttlegentToolsetTool Tool;
			FString Toolset;
			if (!FindCalledTool(Arguments, Tool, Toolset, OutError)) return false;

			OutRequest.Action = Tool.bUndoable
				? FText::Format(LOCTEXT("Action", "调用引擎工具 {0} · {1}（能否撤销以执行结果为准）"), FText::FromString(ShortName(Toolset)), FText::FromString(Tool.Name)).ToString()
				: FText::Format(LOCTEXT("ActionNonUndoable", "调用引擎工具 {0} · {1}"), FText::FromString(ShortName(Toolset)), FText::FromString(Tool.Name)).ToString();
			OutRequest.bUndoable = OutRequest.bUndoable && Tool.bUndoable;
			OutRequest.bDestructive = Tool.bDestructive;
			const UWorld* World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
			if (IsValid(World)) OutRequest.Level = World->GetMapName();

			FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
			Item.Name = Tool.Name;
			Item.Path = Tool.FullName;
			Item.Field = LOCTEXT("ArgumentsField", "参数").ToString();
			Item.CurrentValue = FirstLine(Tool.Description);
			Item.PlannedValue = Truncate(GetToolArguments(Arguments), ShuttlegentToolsetBridgeConstant::MaxArgumentChars);
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			FShuttlegentToolsetTool Tool;
			FString Toolset;
			FString Error;
			const bool bFound = FindCalledTool(Arguments, Tool, Toolset, Error);
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				if (!bFound)
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = Error;
					continue;
				}

				TFuture<TValueOrError<FString, FString>> Future = FShuttlegentToolsets::Execute(Toolset, Tool.Name, GetToolArguments(Arguments));
				if (!Future.IsReady())
				{
					// 工具已在运行；它会在本事务之外完成，且不等待其结果
					Item.Status = EShuttlegentWriteObjectStatus::Completed;
					Item.Message = LOCTEXT("StillRunning", "工具仍在后台执行，结果未返回").ToString();
					continue;
				}

				const TValueOrError<FString, FString> Result = Future.Get();
				if (Result.HasError())
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = Result.GetError();
					continue;
				}

				// 删除或重命名未生效时，资源工具返回 false 而不是报错
				if (FShuttlegentToolsets::ReturnedFalse(Result.GetValue()))
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = ShuttlegentToolsetBridgeConstant::ReturnedFalseError + Result.GetValue();
					continue;
				}

				Item.Status = EShuttlegentWriteObjectStatus::Completed;
				Item.Output = Result.GetValue();
			}
		}
	};
}

void FShuttlegentToolsetTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentToolsetToolsPrivate::FListToolsetsTool>());
	OutTools.Add(MakeShared<FShuttlegentToolsetToolsPrivate::FDescribeToolsetTool>());
	OutTools.Add(MakeShared<FShuttlegentToolsetToolsPrivate::FCallToolsetTool>());
}

#undef LOCTEXT_NAMESPACE
