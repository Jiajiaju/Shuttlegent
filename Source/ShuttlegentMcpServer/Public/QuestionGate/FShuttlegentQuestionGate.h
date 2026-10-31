#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

#include "QuestionGate/EShuttlegentQuestionOutcome.h"
#include "QuestionGate/FShuttlegentQuestion.h"

DECLARE_MULTICAST_DELEGATE(FOnShuttlegentQuestionsChanged);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnShuttlegentQuestionClosed, const FShuttlegentQuestion& /*Question*/, EShuttlegentQuestionOutcome /*Outcome*/, const FString& /*Answer*/);

/**
 * Questions the agent asks the user through ask_user, waiting for the panel to answer them. Works like the write
 * gate: a question waits until the panel answers or skips it once, it expires (the owner expires everything when the
 * turn ends or the runtime is lost) or it times out unanswered. Without an attached panel a question is closed at once
 * as NoPanel, so the agent asks in its reply instead. Game thread only.
 */
class SHUTTLEGENTMCPSERVER_API FShuttlegentQuestionGate : public TSharedFromThis<FShuttlegentQuestionGate>
{
public:
	/** A question arrived or left; the panel shows the first waiting one. */
	FOnShuttlegentQuestionsChanged OnChanged;

	/** A question left the gate, with how and the answer when there is one. */
	FOnShuttlegentQuestionClosed OnClosed;

private:
	/** Waiting questions in arrival order, with the tool's callback. */
	TArray<FShuttlegentQuestion> Pending;
	TMap<int32, TFunction<void(EShuttlegentQuestionOutcome, const FString&)>> Callbacks;

	int32 NextQuestionId = 1;
	bool bAnswererAttached = false;

	/** Checks waiting questions for the time limit; runs only while something waits. */
	FTSTicker::FDelegateHandle TimeoutHandle;

public:
	void Shutdown();

	/** The panel is open to answer questions; detaching closes what still waits as NoPanel. */
	void SetAnswererAttached(bool bAttached);

	/** Asks the user; OnClosed runs once with the outcome and the answer. Returns the question id. */
	int32 Ask(FShuttlegentQuestion Question, TFunction<void(EShuttlegentQuestionOutcome, const FString&)> OnClosedCallback);

	/** The user's answer, or a skip; false when the question is no longer waiting, so a second click does nothing. */
	bool Answer(int32 QuestionId, const FString& InAnswer, bool bSkip);

	/** Every waiting question expires unanswered. */
	void ExpireAll();

	const TArray<FShuttlegentQuestion>& GetPending() const { return Pending; }

private:
	void Close(int32 QuestionId, EShuttlegentQuestionOutcome Outcome, const FString& InAnswer);
	bool HandleTimeoutCheck(float DeltaTime);
};
