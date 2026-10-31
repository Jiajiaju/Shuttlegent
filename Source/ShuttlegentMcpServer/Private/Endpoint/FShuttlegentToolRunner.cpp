#include "Endpoint/FShuttlegentToolRunner.h"

#include "Editor.h"
#include "Editor/Transactor.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "ShuttlegentLog.h"
#include "ShuttlegentMcpServerConstants.h"
#include "Tool/IShuttlegentTool.h"
#include "WriteGate/FShuttlegentWriteGate.h"

#define LOCTEXT_NAMESPACE "ShuttlegentToolRunner"

namespace FShuttlegentToolRunnerPrivate
{
	FString CompactJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Json;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
		FJsonSerializer::Serialize(Object, Writer);
		return Json;
	}

	const TCHAR* StatusWord(EShuttlegentWriteObjectStatus Status)
	{
		switch (Status)
		{
		case EShuttlegentWriteObjectStatus::Completed:
			{
				return TEXT("done");
			}
		case EShuttlegentWriteObjectStatus::Failed:
			{
				return TEXT("failed");
			}
		case EShuttlegentWriteObjectStatus::RolledBack:
			{
				return TEXT("rolled back");
			}
		case EShuttlegentWriteObjectStatus::NotRun:
		default:
			{
				return TEXT("not run");
			}
		}
	}

	/** 模型获知的被拒绝或未答复写入的信息；这些情况下均未写入任何内容。 */
	FString DecisionText(EShuttlegentWriteDecision Decision)
	{
		switch (Decision)
		{
		case EShuttlegentWriteDecision::Rejected:
			{
				return TEXT("The user declined this change (or no confirmation panel was open). Nothing was written. Do not retry unless the user asks.");
			}
		case EShuttlegentWriteDecision::TimedOut:
			{
				return TEXT("The user did not confirm this change in time. Nothing was written. Ask the user before trying again.");
			}
		case EShuttlegentWriteDecision::Expired:
		default:
			{
				return TEXT("This change expired before it was confirmed (the level changed or the turn ended). Nothing was written.");
			}
		}
	}

	/** 每个对象一行，另加一行总体结果，供模型阅读。 */
	FString ResultText(const FShuttlegentWriteResult& Result, bool bRolledBack)
	{
		TArray<FString> Lines;
		int32 Completed = 0;
		for (const FShuttlegentWriteObject& Object : Result.Objects)
		{
			if (Object.Status == EShuttlegentWriteObjectStatus::Completed) ++Completed;
			FString Line = FString::Printf(TEXT("- %s (%s): %s"), *Object.Name, *Object.Path, StatusWord(Object.Status));
			if (!Object.Message.IsEmpty()) Line += TEXT(" - ") + Object.Message;
			if (!Object.Output.IsEmpty()) Line += TEXT("\n  Result: ") + Object.Output;
			Lines.Add(Line);
		}

		FString Outcome;
		if (bRolledBack) Outcome = TEXT("Failed; the whole change was rolled back, nothing remains written.");
		else if (Completed == Result.Objects.Num() && Result.bUndoable) Outcome = FString::Printf(TEXT("Done: %d object(s) written in one undoable step."), Completed);
		else if (Completed == Result.Objects.Num()) Outcome = FString::Printf(TEXT("Done: %d object(s) written."), Completed);
		else Outcome = FString::Printf(TEXT("Partly done: %d of %d object(s) written."), Completed, Result.Objects.Num());
		if (!Result.bUndoable && Completed > 0) Outcome += TEXT(" This change cannot be undone from the editor.");
		return Outcome + TEXT("\n") + FString::Join(Lines, TEXT("\n"));
	}

	/** 写入已确认的对象；事务性写入在任一处失败时整体撤销。 */
	FShuttlegentWriteResult Write(const FShuttlegentWriteRequest& Request, const TSharedRef<IShuttlegentTool>& Tool, const TSharedRef<FJsonObject>& Arguments, bool& bOutRolledBack)
	{
		FShuttlegentWriteResult Result;
		Result.RequestId = Request.RequestId;
		Result.Action = Request.Action;
		Result.bUndoable = Request.bUndoable;
		Result.Objects = Request.Objects;
		bOutRolledBack = false;

		// 非事务性写入，或工具预览发现本次调用不得撤销（撤销会损坏对象）
		if (!Tool->GetDefinition().bTransactional || !Request.bUndoable)
		{
			Tool->Execute(Arguments, Result.Objects);
			return Result;
		}

		// 写入前最新的事务；未记录任何内容的写入会使它仍位于顶部
		const bool bHasTransactor = GEditor != nullptr && IsValid(GEditor->Trans);
		const FGuid PreviousTransactionId = bHasTransactor ? GEditor->Trans->GetUndoContext(true).TransactionId : FGuid();

		bool bAnyWritten = false;
		bool bAnyFailed = false;
		{
			FScopedTransaction Transaction(FText::Format(LOCTEXT("Transaction", "Shuttlegent · {0}"), FText::FromString(Request.Action)));
			Tool->Execute(Arguments, Result.Objects);
			bAnyWritten = Result.Objects.ContainsByPredicate([](const FShuttlegentWriteObject& Object) { return Object.Status == EShuttlegentWriteObjectStatus::Completed; });
			bAnyFailed = Result.Objects.ContainsByPredicate([](const FShuttlegentWriteObject& Object) { return Object.Status != EShuttlegentWriteObjectStatus::Completed; });
			if (!bAnyWritten) Transaction.Cancel();
		}
		if (!bAnyWritten || GEditor == nullptr || !IsValid(GEditor->Trans)) return Result;

		// 未修改任何可撤销内容的工具（如保存文件、新建资产）不会留下事务；此时顶部的事务属于其他操作
		const FGuid TransactionId = GEditor->Trans->GetUndoContext(true).TransactionId;
		if (!TransactionId.IsValid() || TransactionId == PreviousTransactionId)
		{
			Result.bUndoable = false;
			return Result;
		}
		if (!bAnyFailed)
		{
			Result.TransactionId = TransactionId;
			return Result;
		}

		// 不残留部分状态：失败前已写入的对象随事务一并回滚
		bOutRolledBack = FShuttlegentWriteGate::Undo(TransactionId);
		if (!bOutRolledBack)
		{
			UE_LOG(LogShuttlegent, Error, TEXT("Write request %d failed and its transaction could not be undone; check the edited objects."), Request.RequestId);
			Result.TransactionId = TransactionId;
			return Result;
		}

		for (FShuttlegentWriteObject& Object : Result.Objects)
		{
			if (Object.Status != EShuttlegentWriteObjectStatus::Completed) continue;

			Object.Status = EShuttlegentWriteObjectStatus::RolledBack;
			Object.Message = LOCTEXT("RolledBack", "同批其他对象失败，已随整批回滚").ToString();
		}
		return Result;
	}
}

void FShuttlegentToolRunner::Call(const TSharedRef<FShuttlegentWriteGate>& Gate, const TSharedRef<IShuttlegentTool>& Tool, const TSharedRef<FJsonObject>& Arguments, TFunction<void(const FShuttlegentToolResult&)> OnDone)
{
	const FShuttlegentToolDefinition& Definition = Tool->GetDefinition();
	if (Tool->IsReadOnlyCall(Arguments))
	{
		Tool->RunAsync(Arguments, [OnDone](const FShuttlegentToolResult& Result)
		{
			FShuttlegentToolResult Limited = Result;
			Limited.Text = LimitOutput(Limited.Text);
			OnDone(Limited);
		});
		return;
	}

	FShuttlegentWriteRequest Request;
	Request.ToolName = Definition.Name;
	Request.bUndoable = Definition.bTransactional;
	Request.RawArguments = FShuttlegentToolRunnerPrivate::CompactJson(Arguments);
	FString Error;
	if (!Tool->Preview(Arguments, Request, Error))
	{
		OnDone(FShuttlegentToolResult::Error(Error));
		return;
	}
	if (Request.Objects.IsEmpty())
	{
		OnDone(FShuttlegentToolResult::Error(TEXT("Nothing to change: the call matched no objects.")));
		return;
	}

	// 由写入门为请求编号；决定可能在 Request 返回之前到达（未打开面板时）
	const TSharedRef<FShuttlegentWriteRequest> Confirmed = MakeShared<FShuttlegentWriteRequest>(Request);
	const TWeakPtr<FShuttlegentWriteGate> WeakGate = Gate;
	const int32 RequestId = Gate->Request(MoveTemp(Request), [WeakGate, Confirmed, Tool, Arguments, OnDone](EShuttlegentWriteDecision Decision)
	{
		if (Decision != EShuttlegentWriteDecision::Approved)
		{
			OnDone(FShuttlegentToolResult::Error(FShuttlegentToolRunnerPrivate::DecisionText(Decision)));
			return;
		}

		bool bRolledBack = false;
		const FShuttlegentWriteResult Result = FShuttlegentToolRunnerPrivate::Write(*Confirmed, Tool, Arguments, bRolledBack);
		const TSharedPtr<FShuttlegentWriteGate> PinnedGate = WeakGate.Pin();
		if (PinnedGate.IsValid()) PinnedGate->ReportResult(Result);

		const bool bAllDone = !Result.Objects.ContainsByPredicate([](const FShuttlegentWriteObject& Object) { return Object.Status != EShuttlegentWriteObjectStatus::Completed; });
		OnDone(FShuttlegentToolResult { LimitOutput(FShuttlegentToolRunnerPrivate::ResultText(Result, bRolledBack)), !bAllDone });
	});
	Confirmed->RequestId = RequestId;
}

FString FShuttlegentToolRunner::LimitOutput(const FString& Text)
{
	if (Text.Len() <= ShuttlegentMcpTool::MaxOutputChars) return Text;

	return Text.Left(ShuttlegentMcpTool::MaxOutputChars) + FString::Printf(TEXT("\n[Output cut at %d of %d characters. Ask for less: a narrower filter or a smaller limit.]"), ShuttlegentMcpTool::MaxOutputChars, Text.Len());
}

#undef LOCTEXT_NAMESPACE
