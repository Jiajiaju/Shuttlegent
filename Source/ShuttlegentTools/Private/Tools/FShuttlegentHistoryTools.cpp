#include "Tools/FShuttlegentHistoryTools.h"

#include "Editor.h"
#include "Editor/Transactor.h"

#include "FShuttlegentToolBase.h"
#include "ShuttlegentToolsConstants.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"
#include "WriteGate/FShuttlegentWriteRequest.h"

#define LOCTEXT_NAMESPACE "ShuttlegentHistoryTools"

namespace FShuttlegentHistoryToolsPrivate
{
	// Tool names
	inline const TCHAR* UndoName = TEXT("undo");
	inline const TCHAR* RedoName = TEXT("redo");

	// Argument names
	inline const TCHAR* ArgSteps = TEXT("steps");

	/**
	 * Undo or redo of the editor's transaction history. The steps are the editor's own transactions, so the history
	 * itself is the undo of this tool: an undo is reverted by redo and the other way round; it opens no transaction.
	 */
	class FHistoryTool : public FShuttlegentToolBase
	{
	private:
		bool bRedo = false;

	public:
		explicit FHistoryTool(bool bInRedo)
			: bRedo(bInRedo)
		{
			Definition.Name = bRedo ? RedoName : UndoName;
			Definition.Description = bRedo
				? TEXT("Redo the editor's most recently undone steps, as Ctrl+Y would. The user confirms first and sees which steps.")
				: TEXT("Undo the editor's most recent steps, as Ctrl+Z would; this includes steps the user made by hand. The user confirms first and sees which steps.");
			Definition.bReadOnly = false;
			Definition.bTransactional = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.Integer(ArgSteps, TEXT("How many steps, 1-20. Defaults to 1."))
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			UTransactor* Transactor = GEditor != nullptr ? GEditor->Trans.Get() : nullptr;
			if (!IsValid(Transactor))
			{
				OutError = TEXT("The editor has no undo history.");
				return false;
			}

			// Undoable steps are the queue before the undone ones; the next undo is the newest of them, the next redo the oldest undone
			const int32 Steps = FMath::Clamp(FShuttlegentToolArguments::GetInteger(Arguments, ArgSteps, 1), 1, ShuttlegentToolsConstant::MaxHistorySteps);
			const int32 FirstUndone = Transactor->GetQueueLength() - Transactor->GetUndoCount();
			OutRequest.Action = bRedo ? LOCTEXT("RedoAction", "重做编辑器操作").ToString() : LOCTEXT("UndoAction", "撤销编辑器操作").ToString();
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const int32 QueueIndex = bRedo ? FirstUndone + Step : FirstUndone - 1 - Step;
				const FTransaction* Transaction = Transactor->GetTransaction(QueueIndex);
				if (Transaction == nullptr) break;

				FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
				Item.Name = Transaction->GetTitle().ToString();
				Item.Field = bRedo ? LOCTEXT("RedoField", "重做").ToString() : LOCTEXT("UndoField", "撤销").ToString();
				Item.CurrentValue = FText::Format(LOCTEXT("StepDone", "第 {0} 步"), FText::AsNumber(Step + 1)).ToString();
				Item.PlannedValue = bRedo ? LOCTEXT("Redone", "重新执行").ToString() : LOCTEXT("Undone", "撤销").ToString();
			}
			if (OutRequest.Objects.IsEmpty()) OutError = bRedo ? TEXT("Nothing to redo.") : TEXT("Nothing to undo.");
			return !OutRequest.Objects.IsEmpty();
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				const bool bDone = GEditor != nullptr && (bRedo ? GEditor->RedoTransaction() : GEditor->UndoTransaction());
				Item.Status = bDone ? EShuttlegentWriteObjectStatus::Completed : EShuttlegentWriteObjectStatus::Failed;
				if (bDone) continue;

				// The history changed since the preview; the remaining steps are not taken either
				Item.Message = LOCTEXT("HistoryChanged", "编辑器历史已变化，未执行").ToString();
				break;
			}
		}
	};
}

void FShuttlegentHistoryTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentHistoryToolsPrivate::FHistoryTool>(false));
	OutTools.Add(MakeShared<FShuttlegentHistoryToolsPrivate::FHistoryTool>(true));
}

#undef LOCTEXT_NAMESPACE
