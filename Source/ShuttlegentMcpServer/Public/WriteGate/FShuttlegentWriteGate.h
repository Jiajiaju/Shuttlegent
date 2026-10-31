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
 * 编辑器写入工具的权限门（Shuttlegent.md 第 10.3 节）：每次写入在运行前都须在此请求。
 *
 * 请求一直等待，直到确认 UI 对其答复一次（批准或拒绝）、请求过期（关卡切换，或运行时丢失、本轮结束时由所有者
 * 令全部请求过期），或无人作答而超时。未挂接确认 UI 时，请求立即被拒绝：未经确认不写入任何内容。批准的写入完成后，
 * 工具逐个对象上报实际执行结果及其事务，据此判断“撤销本次修改”是否仍然可行。仅限游戏线程。
 */
class SHUTTLEGENTMCPSERVER_API FShuttlegentWriteGate : public TSharedFromThis<FShuttlegentWriteGate>
{
public:
	FOnShuttlegentWriteRequested OnRequested;
	FOnShuttlegentWriteClosed OnClosed;
	FOnShuttlegentWriteFinished OnFinished;

private:
	/** 按到达顺序排列的等待中请求，及工具的回调。 */
	TArray<FShuttlegentWriteRequest> Pending;
	TMap<int32, TFunction<void(EShuttlegentWriteDecision)>> Callbacks;

	int32 NextRequestId = 1;
	bool bConfirmerAttached = false;
	FDelegateHandle MapChangeHandle;

	/** 检查等待中请求是否超出确认时限；仅在有请求等待时运行。 */
	FTSTicker::FDelegateHandle TimeoutHandle;

public:
	void Initialize();
	void Shutdown();

	/** 面板已打开，可确认写入；解除挂接期间所有请求均被拒绝。解除挂接时拒绝仍在等待的请求。 */
	void SetConfirmerAttached(bool bAttached);
	bool IsConfirmerAttached() const { return bConfirmerAttached; }

	/** 请求确认；OnDecision 以结果运行一次。返回请求 id。 */
	int32 Request(FShuttlegentWriteRequest Request, TFunction<void(EShuttlegentWriteDecision)> OnDecision);

	/** 用户的答复；请求已不在等待（已答复、已过期）时返回 false，因此重复点击不产生作用。 */
	bool Answer(int32 RequestId, bool bApprove);

	/** 令所有等待中的请求过期，不执行写入。 */
	void ExpireAll();

	const TArray<FShuttlegentWriteRequest>& GetPending() const { return Pending; }

	/** 工具上报一次已批准写入的执行结果。 */
	void ReportResult(const FShuttlegentWriteResult& Result);

	/** 该事务是编辑器下一次撤销的对象（其后没有执行或撤销过任何操作）。 */
	static bool IsOnUndoTop(const FGuid& TransactionId);

	/** 该事务位于撤销缓冲区顶部时将其撤销；否则返回 false。 */
	static bool Undo(const FGuid& TransactionId);

private:
	void Close(int32 RequestId, EShuttlegentWriteDecision Decision);
	void HandleMapChange(uint32 Flags);
	bool HandleTimeoutCheck(float DeltaTime);
};
