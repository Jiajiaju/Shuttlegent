#include "Tools/FShuttlegentInteractionTools.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "FShuttlegentToolBase.h"
#include "QuestionGate/FShuttlegentQuestionGate.h"
#include "ShuttlegentMcpServerModule.h"
#include "ShuttlegentToolsConstants.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"

namespace FShuttlegentInteractionToolsPrivate
{
	// 工具名称
	inline const TCHAR* AskUserName = TEXT("ask_user");

	// 参数名称
	inline const TCHAR* ArgQuestion = TEXT("question");
	inline const TCHAR* ArgOptions = TEXT("options");
	inline const TCHAR* ArgAllowOther = TEXT("allow_other");

	// ask_choices 的名称与参数
	inline const TCHAR* AskChoicesName = TEXT("ask_choices");
	inline const TCHAR* ArgMultiSelect = TEXT("multi_select");
	inline const TCHAR* ArgItemNotes = TEXT("item_notes");
	inline const TCHAR* ArgOverallNote = TEXT("overall_note");
	inline const TCHAR* ArgOptionId = TEXT("id");
	inline const TCHAR* ArgOptionTitle = TEXT("title");
	inline const TCHAR* ArgOptionDescription = TEXT("description");
	inline const TCHAR* ArgOptionImage = TEXT("image");

	/** 选择卡已显示时 Agent 得到的反馈：本轮就此结束，用户的选择作为下一条消息到来。 */
	inline const TCHAR* ChoiceShownText = TEXT("The choice card is shown in the panel. End your turn now with one short sentence telling the user to choose on the card; do not guess their choice or keep working. Their choice arrives as their next message, naming each picked option by its id, with any notes.");

	/** 没有面板可显示选择卡时 Agent 得到的反馈。 */
	inline const TCHAR* ChoiceNoPanelText = TEXT("No Shuttlegent panel is open to show the choice card. List the options in your reply instead (images as ![caption](src)) and end your turn.");

	/** 选项对象中的一个字符串字段，去掉首尾空白并截到 MaxChars 个字符。 */
	FString GetOptionField(const TSharedPtr<FJsonObject>& Item, const TCHAR* Name, int32 MaxChars)
	{
		FString Value;
		Item->TryGetStringField(Name, Value);
		return Value.TrimStartAndEnd().Left(MaxChars);
	}

	/** 选项数组；id 重复或缺标题时返回 false 并给出原因。缺 id 时按位置补 1、2、3…… */
	bool ParseChoiceOptions(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentChoiceOption>& OutOptions, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Arguments->TryGetArrayField(ArgOptions, Values) || Values == nullptr)
		{
			OutError = TEXT("Pass options as an array of objects.");
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Item.IsValid()) continue;

			FShuttlegentChoiceOption Option;
			Option.Id = GetOptionField(Item, ArgOptionId, ShuttlegentToolsConstant::MaxChoiceIdChars);
			Option.Title = GetOptionField(Item, ArgOptionTitle, ShuttlegentToolsConstant::MaxChoiceTitleChars);
			Option.Description = GetOptionField(Item, ArgOptionDescription, ShuttlegentToolsConstant::MaxChoiceDescriptionChars);
			Item->TryGetStringField(ArgOptionImage, Option.Image);
			Option.Image.TrimStartAndEndInline();
			if (Option.Id.IsEmpty()) Option.Id = FString::FromInt(OutOptions.Num() + 1);

			if (Option.Title.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Option %s has no title."), *Option.Id);
				return false;
			}
			if (OutOptions.ContainsByPredicate([&Option](const FShuttlegentChoiceOption& Existing) { return Existing.Id == Option.Id; }))
			{
				OutError = FString::Printf(TEXT("Option id %s is used twice; give every option its own id."), *Option.Id);
				return false;
			}
			OutOptions.Add(MoveTemp(Option));
		}
		return true;
	}

	/** 问题以各种方式结束时，Agent 得到的反馈文本。 */
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

	/** 在面板中向用户提问并返回其选择；不修改编辑器中的任何内容。 */
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

	/** 在对话中放一张选择卡（可带图片、多选、逐项评语与整体意见）后立即返回；用户的选择作为下一条消息到来。 */
	class FAskChoicesTool : public FShuttlegentToolBase
	{
	public:
		FAskChoicesTool()
		{
			Definition.Name = AskChoicesName;
			Definition.Description = TEXT("Show the user a choice card in the editor panel when they should pick between visual or design options: icons, colours, materials, meshes, alternative plans, or items to review one by one. Each option can carry an image (ue-asset:<object path> for an asset thumbnail, an image file under Saved/Shuttlegent/, or an http(s) URL), and the card can allow several picks, a note per option and an overall note. The call does not wait: end your turn right after it. The user's choice comes back as their next message. For a quick question that decides a single value or object, use ask_user instead.");
			Definition.bReadOnly = true;

			const FShuttlegentToolSchema Option = FShuttlegentToolSchema()
				.String(ArgOptionId, TEXT("Short stable id, unique in this card; the answer names options by it."), true)
				.String(ArgOptionTitle, TEXT("Short title in the user's language."), true)
				.String(ArgOptionDescription, TEXT("Optional one-line description."))
				.String(ArgOptionImage, TEXT("Optional image: ue-asset:<object path> (thumbnail), a path under Saved/Shuttlegent/, or an http(s) URL."));
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgQuestion, TEXT("What the user should decide, short and in the user's language."), true)
				.ObjectArray(ArgOptions, TEXT("The options in display order."), Option, ShuttlegentToolsConstant::MinChoiceOptions, ShuttlegentToolsConstant::MaxChoiceOptions, true)
				.Boolean(ArgMultiSelect, TEXT("Whether the user may pick several options. Defaults to false."))
				.Boolean(ArgItemNotes, TEXT("Whether the user may write a note on each option, e.g. for reviewing items one by one. Defaults to false."))
				.Boolean(ArgOverallNote, TEXT("Whether the user may write an overall note. Defaults to true."))
				.Build();
		}

		virtual void RunAsync(const TSharedRef<FJsonObject>& Arguments, TFunction<void(const FShuttlegentToolResult&)> OnDone) override
		{
			FShuttlegentChoiceRequest Request;
			Request.Question = FShuttlegentToolArguments::GetString(Arguments, ArgQuestion).TrimStartAndEnd();
			Request.bMultiSelect = FShuttlegentToolArguments::GetBoolean(Arguments, ArgMultiSelect, false);
			Request.bItemNotes = FShuttlegentToolArguments::GetBoolean(Arguments, ArgItemNotes, false);
			Request.bOverallNote = FShuttlegentToolArguments::GetBoolean(Arguments, ArgOverallNote, true);
			if (Request.Question.IsEmpty())
			{
				OnDone(FShuttlegentToolResult::Error(TEXT("Pass the question to ask.")));
				return;
			}

			FString Error;
			if (!ParseChoiceOptions(Arguments, Request.Options, Error))
			{
				OnDone(FShuttlegentToolResult::Error(Error));
				return;
			}
			if (Request.Options.Num() < ShuttlegentToolsConstant::MinChoiceOptions || Request.Options.Num() > ShuttlegentToolsConstant::MaxChoiceOptions)
			{
				OnDone(FShuttlegentToolResult::Error(FString::Printf(TEXT("Pass %d-%d options."), ShuttlegentToolsConstant::MinChoiceOptions, ShuttlegentToolsConstant::MaxChoiceOptions)));
				return;
			}

			const TSharedPtr<FShuttlegentQuestionGate> Gate = FShuttlegentMcpServerModule::Get().GetQuestionGate();
			const bool bShown = Gate.IsValid() && Gate->PresentChoice(MoveTemp(Request));
			OnDone(FShuttlegentToolResult::Ok(bShown ? ChoiceShownText : ChoiceNoPanelText));
		}
	};
}

void FShuttlegentInteractionTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentInteractionToolsPrivate::FAskUserTool>());
	OutTools.Add(MakeShared<FShuttlegentInteractionToolsPrivate::FAskChoicesTool>());
}
