#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

#include "WriteGate/EShuttlegentWriteDecision.h"
#include "WriteGate/FShuttlegentWriteRequest.h"
#include "WriteGate/FShuttlegentWriteResult.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentWriteRequested, const FShuttlegentWriteRequest& /*Request*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentWriteClosed, int32 /*RequestId*/, EShuttlegentWriteDecision /*Decision*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentWriteFinished, const FShuttlegentWriteResult& /*Result*/);

/**
 * Permission gate of editor write tools (Shuttlegent.md section 10.3): every write asks here before it runs.
 *
 * A request waits until the confirmation UI answers it once (approved or rejected), it expires (the level changes,
 * or the owner expires everything when the runtime is lost or the turn ends) or it times out unanswered. Without an attached confirmation UI a
 * request is rejected at once: nothing writes unconfirmed. After an approved write the tool reports what it actually
 * did, object by object, with its transaction, which decides whether "undo this change" is still possible. Game thread only.
 */
class SHUTTLEGENTMCPSERVER_API FShuttlegentWriteGate : public TSharedFromThis<FShuttlegentWriteGate>
{
public:
	FOnShuttlegentWriteRequested OnRequested;
	FOnShuttlegentWriteClosed OnClosed;
	FOnShuttlegentWriteFinished OnFinished;

private:
	/** Waiting requests in arrival order, with the tool's callback. */
	TArray<FShuttlegentWriteRequest> Pending;
	TMap<int32, TFunction<void(EShuttlegentWriteDecision)>> Callbacks;

	int32 NextRequestId = 1;
	bool bConfirmerAttached = false;
	FDelegateHandle MapChangeHandle;

	/** Checks waiting requests for the confirmation timeout; runs only while something waits. */
	FTSTicker::FDelegateHandle TimeoutHandle;

public:
	void Initialize();
	void Shutdown();

	/** The panel is open to confirm writes; while detached every request is rejected. Detaching rejects what still waits. */
	void SetConfirmerAttached(bool bAttached);
	bool IsConfirmerAttached() const { return bConfirmerAttached; }

	/** Asks for confirmation; OnDecision runs once with the outcome. Returns the request id. */
	int32 Request(FShuttlegentWriteRequest Request, TFunction<void(EShuttlegentWriteDecision)> OnDecision);

	/** The user's answer; false when the request is no longer waiting (answered, expired), so a second click does nothing. */
	bool Answer(int32 RequestId, bool bApprove);

	/** Every waiting request expires without writing. */
	void ExpireAll();

	const TArray<FShuttlegentWriteRequest>& GetPending() const { return Pending; }

	/** The tool reports what an approved write did. */
	void ReportResult(const FShuttlegentWriteResult& Result);

	/** The transaction is the next one the editor would undo (nothing was done or undone after it). */
	static bool IsOnUndoTop(const FGuid& TransactionId);

	/** Undoes the transaction when it is on top of the undo buffer; false otherwise. */
	static bool Undo(const FGuid& TransactionId);

private:
	void Close(int32 RequestId, EShuttlegentWriteDecision Decision);
	void HandleMapChange(uint32 Flags);
	bool HandleTimeoutCheck(float DeltaTime);
};
