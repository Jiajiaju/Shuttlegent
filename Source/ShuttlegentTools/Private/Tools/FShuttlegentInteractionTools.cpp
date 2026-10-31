#include "Tools/FShuttlegentInteractionTools.h"

#include "FShuttlegentToolBase.h"
#include "QuestionGate/FShuttlegentQuestionGate.h"
#include "ShuttlegentMcpServerModule.h"
#include "ShuttlegentToolsConstants.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"

namespace FShuttlegentInteractionToolsPrivate
{
	// Tool name
	inline const TCHAR* AskUserName = TEXT("ask_user");

	// Argument names
	inline const TCHAR* ArgQuestion = TEXT("question");
	inline const TCHAR* ArgOptions = TEXT("options");
	inline const TCHAR* ArgAllowOther = TEXT("allow_other");

	/** What the agent learns for each way a question can end. */
	FString OutcomeText(EShuttlegentQuestionOutcome Outcome, const FString& Answer)
	{
		switch (Outcome)
		{
		case EShuttlegentQuestionOutcome::Answered:
			{
				return FString::Printf(TEXT("The user answered: \"%s\""), *Answer);
			}
		case EShuttlegentQuestionOutcome::Skipped:
			{
				return TEXT("The user closed the question without answering. Do not guess: do only what is certain, or ask in your reply and end your turn.");
			}
		case EShuttlegentQuestionOutcome::TimedOut:
			{
				return TEXT("The user did not answer in time. Ask the question in your reply instead and end your turn.");
			}
		case EShuttlegentQuestionOutcome::NoPanel:
			{
				return TEXT("No Shuttlegent panel is open to show the question. Ask it in your reply instead and end your turn.");
			}
		case EShuttlegentQuestionOutcome::Expired:
		default:
			{
				return TEXT("The question was withdrawn before the user answered (the turn ended or the runtime restarted).");
			}
		}
	}

	/** Asks the user in the panel and answers with their choice; it changes nothing in the editor. */
	class FAskUserTool : public FShuttlegentToolBase
	{
	public:
		FAskUserTool()
		{
			Definition.Name = AskUserName;
			Definition.Description = TEXT("Ask the user to choose before you act, when a request is ambiguous and the choice decides what changes (which object, which value, which of several matches). The user sees the question with your options as buttons in the editor panel and must answer within about a minute; the result is their answer. Do not use it for questions you can settle with read tools.");
			Definition.bReadOnly = true;
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgQuestion, TEXT("The question, short and in the user's language."), true)
				.StringArray(ArgOptions, TEXT("2-6 short answers to choose from, in the user's language, most likely first."), true)
				.Boolean(ArgAllowOther, TEXT("Whether the user may write an answer of their own. Defaults to true."))
				.Build();
		}

		virtual void RunAsync(const TSharedRef<FJsonObject>& Arguments, TFunction<void(const FShuttlegentToolResult&)> OnDone) override
		{
			FShuttlegentQuestion Question;
			Question.Question = FShuttlegentToolArguments::GetString(Arguments, ArgQuestion).TrimStartAndEnd();
			Question.bAllowOther = FShuttlegentToolArguments::GetBoolean(Arguments, ArgAllowOther, true);
			for (const FString& Option : FShuttlegentToolArguments::GetStringArray(Arguments, ArgOptions))
			{
				const FString Trimmed = Option.TrimStartAndEnd();
				if (!Trimmed.IsEmpty()) Question.Options.AddUnique(Trimmed);
			}

			if (Question.Question.IsEmpty())
			{
				OnDone(FShuttlegentToolResult::Error(TEXT("Pass the question to ask.")));
				return;
			}
			if (Question.Options.Num() < ShuttlegentToolsConstant::MinQuestionOptions || Question.Options.Num() > ShuttlegentToolsConstant::MaxQuestionOptions)
			{
				OnDone(FShuttlegentToolResult::Error(FString::Printf(TEXT("Pass %d-%d different options."), ShuttlegentToolsConstant::MinQuestionOptions, ShuttlegentToolsConstant::MaxQuestionOptions)));
				return;
			}

			const TSharedPtr<FShuttlegentQuestionGate> Gate = FShuttlegentMcpServerModule::Get().GetQuestionGate();
			if (!Gate.IsValid())
			{
				OnDone(FShuttlegentToolResult::Ok(OutcomeText(EShuttlegentQuestionOutcome::NoPanel, FString())));
				return;
			}

			Gate->Ask(MoveTemp(Question), [OnDone](EShuttlegentQuestionOutcome Outcome, const FString& Answer)
			{
				OnDone(FShuttlegentToolResult::Ok(OutcomeText(Outcome, Answer)));
			});
		}
	};
}

void FShuttlegentInteractionTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentInteractionToolsPrivate::FAskUserTool>());
}
