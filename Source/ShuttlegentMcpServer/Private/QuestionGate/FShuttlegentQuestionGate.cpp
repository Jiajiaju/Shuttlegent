#include "QuestionGate/FShuttlegentQuestionGate.h"

#include "ShuttlegentLog.h"
#include "ShuttlegentMcpServerConstants.h"

namespace FShuttlegentQuestionGatePrivate
{
	const TCHAR* OutcomeName(EShuttlegentQuestionOutcome Outcome)
	{
		switch (Outcome)
		{
		case EShuttlegentQuestionOutcome::Answered:
			{
				return TEXT("answered");
			}
		case EShuttlegentQuestionOutcome::Skipped:
			{
				return TEXT("skipped");
			}
		case EShuttlegentQuestionOutcome::TimedOut:
			{
				return TEXT("timed out");
			}
		case EShuttlegentQuestionOutcome::NoPanel:
			{
				return TEXT("no panel");
			}
		case EShuttlegentQuestionOutcome::Expired:
		default:
			{
				return TEXT("expired");
			}
		}
	}
}

void FShuttlegentQuestionGate::Shutdown()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
	TimeoutHandle.Reset();
	ExpireAll();
}

void FShuttlegentQuestionGate::SetAnswererAttached(bool bAttached)
{
	if (bAnswererAttached == bAttached) return;

	bAnswererAttached = bAttached;
	if (bAttached) return;

	// 已无人可作答：agent 改为在回复中提问
	while (Pending.Num() > 0)
	{
		Close(Pending[0].QuestionId, EShuttlegentQuestionOutcome::NoPanel, FString());
	}
}

int32 FShuttlegentQuestionGate::Ask(FShuttlegentQuestion Question, TFunction<void(EShuttlegentQuestionOutcome, const FString&)> OnClosedCallback)
{
	Question.QuestionId = NextQuestionId++;
	const int32 QuestionId = Question.QuestionId;
	if (!bAnswererAttached)
	{
		UE_LOG(LogShuttlegent, Log, TEXT("Question %d not shown: no Shuttlegent panel is open."), QuestionId);
		if (OnClosedCallback) OnClosedCallback(EShuttlegentQuestionOutcome::NoPanel, FString());
		return QuestionId;
	}

	UE_LOG(LogShuttlegent, Log, TEXT("Question %d waits for an answer: %d option(s)."), QuestionId, Question.Options.Num());
	Question.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromSeconds(ShuttlegentWriteGateConstant::ConfirmTimeoutSeconds);
	Callbacks.Add(QuestionId, MoveTemp(OnClosedCallback));
	Pending.Add(MoveTemp(Question));
	if (!TimeoutHandle.IsValid()) TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FShuttlegentQuestionGate::HandleTimeoutCheck), ShuttlegentWriteGateConstant::TimeoutCheckSeconds);
	OnChanged.Broadcast();
	return QuestionId;
}

bool FShuttlegentQuestionGate::Answer(int32 QuestionId, const FString& InAnswer, bool bSkip)
{
	if (!Callbacks.Contains(QuestionId)) return false;

	const FString Trimmed = InAnswer.TrimStartAndEnd();
	if (!bSkip && Trimmed.IsEmpty()) return false;

	Close(QuestionId, bSkip ? EShuttlegentQuestionOutcome::Skipped : EShuttlegentQuestionOutcome::Answered, bSkip ? FString() : Trimmed);
	return true;
}

void FShuttlegentQuestionGate::ExpireAll()
{
	while (Pending.Num() > 0)
	{
		Close(Pending[0].QuestionId, EShuttlegentQuestionOutcome::Expired, FString());
	}
}

void FShuttlegentQuestionGate::Close(int32 QuestionId, EShuttlegentQuestionOutcome Outcome, const FString& InAnswer)
{
	TFunction<void(EShuttlegentQuestionOutcome, const FString&)> Callback;
	Callbacks.RemoveAndCopyValue(QuestionId, Callback);

	FShuttlegentQuestion Question;
	const int32 Index = Pending.IndexOfByPredicate([QuestionId](const FShuttlegentQuestion& Item) { return Item.QuestionId == QuestionId; });
	if (Index != INDEX_NONE)
	{
		Question = Pending[Index];
		Pending.RemoveAt(Index);
	}

	UE_LOG(LogShuttlegent, Log, TEXT("Question %d closed: %s."), QuestionId, FShuttlegentQuestionGatePrivate::OutcomeName(Outcome));

	OnClosed.Broadcast(Question, Outcome, InAnswer);
	OnChanged.Broadcast();
	if (Callback) Callback(Outcome, InAnswer);
}

bool FShuttlegentQuestionGate::HandleTimeoutCheck(float DeltaTime)
{
	const FDateTime Now = FDateTime::UtcNow();
	TArray<int32> TimedOut;
	for (const FShuttlegentQuestion& Item : Pending)
	{
		if (Item.ExpiresAtUtc <= Now) TimedOut.Add(Item.QuestionId);
	}
	for (const int32 QuestionId : TimedOut)
	{
		Close(QuestionId, EShuttlegentQuestionOutcome::TimedOut, FString());
	}

	if (!Pending.IsEmpty()) return true;

	TimeoutHandle.Reset();
	return false;
}
