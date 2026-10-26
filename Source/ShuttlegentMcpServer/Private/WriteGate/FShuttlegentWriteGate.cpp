#include "WriteGate/FShuttlegentWriteGate.h"

#include "Editor.h"
#include "Editor/Transactor.h"

#include "ShuttlegentLog.h"
#include "ShuttlegentMcpServerConstants.h"

namespace FShuttlegentWriteGatePrivate
{
	const TCHAR* DecisionName(EShuttlegentWriteDecision Decision)
	{
		switch (Decision)
		{
		case EShuttlegentWriteDecision::Approved:
			{
				return TEXT("approved");
			}
		case EShuttlegentWriteDecision::Rejected:
			{
				return TEXT("rejected");
			}
		case EShuttlegentWriteDecision::TimedOut:
			{
				return TEXT("timed out");
			}
		case EShuttlegentWriteDecision::Expired:
		default:
			{
				return TEXT("expired");
			}
		}
	}
}

void FShuttlegentWriteGate::Initialize()
{
	// A level change invalidates every preview: the objects and their current values belong to the old level
	MapChangeHandle = FEditorDelegates::MapChange.AddSP(this, &FShuttlegentWriteGate::HandleMapChange);
}

void FShuttlegentWriteGate::Shutdown()
{
	FEditorDelegates::MapChange.Remove(MapChangeHandle);
	MapChangeHandle.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
	TimeoutHandle.Reset();
	ExpireAll();
}

void FShuttlegentWriteGate::SetConfirmerAttached(bool bAttached)
{
	if (bConfirmerAttached == bAttached) return;

	bConfirmerAttached = bAttached;
	if (bAttached) return;

	// Nobody is left to answer: what still waits is refused, never run unconfirmed
	while (Pending.Num() > 0)
	{
		Close(Pending[0].RequestId, EShuttlegentWriteDecision::Rejected);
	}
}

int32 FShuttlegentWriteGate::Request(FShuttlegentWriteRequest Request, TFunction<void(EShuttlegentWriteDecision)> OnDecision)
{
	Request.RequestId = NextRequestId++;
	const int32 RequestId = Request.RequestId;
	if (!bConfirmerAttached)
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Write request %d (%s) rejected: no confirmation panel is open."), RequestId, *Request.ToolName);
		if (OnDecision) OnDecision(EShuttlegentWriteDecision::Rejected);
		return RequestId;
	}

	UE_LOG(LogShuttlegent, Log, TEXT("Write request %d (%s) waits for confirmation: %d object(s)."), RequestId, *Request.ToolName, Request.Objects.Num());
	Request.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromSeconds(ShuttlegentWriteGateConstant::ConfirmTimeoutSeconds);
	Callbacks.Add(RequestId, MoveTemp(OnDecision));
	Pending.Add(Request);
	if (!TimeoutHandle.IsValid()) TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FShuttlegentWriteGate::HandleTimeoutCheck), ShuttlegentWriteGateConstant::TimeoutCheckSeconds);
	OnRequested.Broadcast(Request);
	return RequestId;
}

bool FShuttlegentWriteGate::Answer(int32 RequestId, bool bApprove)
{
	if (!Callbacks.Contains(RequestId)) return false;

	Close(RequestId, bApprove ? EShuttlegentWriteDecision::Approved : EShuttlegentWriteDecision::Rejected);
	return true;
}

void FShuttlegentWriteGate::ExpireAll()
{
	while (Pending.Num() > 0)
	{
		Close(Pending[0].RequestId, EShuttlegentWriteDecision::Expired);
	}
}

void FShuttlegentWriteGate::ReportResult(const FShuttlegentWriteResult& Result)
{
	UE_LOG(LogShuttlegent, Log, TEXT("Write request %d finished: %d object(s), transaction %s."), Result.RequestId, Result.Objects.Num(), *Result.TransactionId.ToString());
	OnFinished.Broadcast(Result);
}

bool FShuttlegentWriteGate::IsOnUndoTop(const FGuid& TransactionId)
{
	if (!TransactionId.IsValid() || GEditor == nullptr || !IsValid(GEditor->Trans)) return false;

	// The undo context names the transaction the next undo would revert; empty when nothing can be undone
	const FTransactionContext Context = GEditor->Trans->GetUndoContext(true);
	return Context.TransactionId == TransactionId;
}

bool FShuttlegentWriteGate::Undo(const FGuid& TransactionId)
{
	if (!IsOnUndoTop(TransactionId)) return false;

	return GEditor->UndoTransaction();
}

void FShuttlegentWriteGate::Close(int32 RequestId, EShuttlegentWriteDecision Decision)
{
	TFunction<void(EShuttlegentWriteDecision)> Callback;
	Callbacks.RemoveAndCopyValue(RequestId, Callback);
	Pending.RemoveAll([RequestId](const FShuttlegentWriteRequest& Item) { return Item.RequestId == RequestId; });

	UE_LOG(LogShuttlegent, Log, TEXT("Write request %d closed: %s."), RequestId, FShuttlegentWriteGatePrivate::DecisionName(Decision));

	// Listeners learn the outcome before the tool acts on it, so the panel shows "writing" while it runs
	OnClosed.Broadcast(RequestId, Decision);
	if (Callback) Callback(Decision);
}

void FShuttlegentWriteGate::HandleMapChange(uint32 Flags)
{
	ExpireAll();
}

bool FShuttlegentWriteGate::HandleTimeoutCheck(float DeltaTime)
{
	const FDateTime Now = FDateTime::UtcNow();
	TArray<int32> TimedOut;
	for (const FShuttlegentWriteRequest& Item : Pending)
	{
		if (Item.ExpiresAtUtc <= Now) TimedOut.Add(Item.RequestId);
	}
	for (const int32 RequestId : TimedOut)
	{
		Close(RequestId, EShuttlegentWriteDecision::TimedOut);
	}

	if (!Pending.IsEmpty()) return true;

	TimeoutHandle.Reset();
	return false;
}
