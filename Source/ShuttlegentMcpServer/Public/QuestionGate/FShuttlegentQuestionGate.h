#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

#include "QuestionGate/EShuttlegentQuestionOutcome.h"
#include "QuestionGate/FShuttlegentQuestion.h"

DECLARE_MULTICAST_DELEGATE(FOnShuttlegentQuestionsChanged);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnShuttlegentQuestionClosed, const FShuttlegentQuestion& /*Question*/, EShuttlegentQuestionOutcome /*Outcome*/, const FString& /*Answer*/);

/**
 * agent 通过 ask_user 向用户提出的问题，等待面板作答。工作方式与写入门相同：问题一直等待，直到面板对其
 * 作答或跳过一次、问题过期（本轮结束或运行时丢失时由所有者令全部问题过期），或无人作答而超时。未挂接面板时，
 * 问题立即以 NoPanel 关闭，agent 改为在回复中提问。仅限游戏线程。
 */
class SHUTTLEGENTMCPSERVER_API FShuttlegentQuestionGate : public TSharedFromThis<FShuttlegentQuestionGate>
{
public:
	/** 有问题到达或离开；面板显示第一个等待中的问题。 */
	FOnShuttlegentQuestionsChanged OnChanged;

	/** 有问题离开提问通道，附带离开方式及答案（如有）。 */
	FOnShuttlegentQuestionClosed OnClosed;

private:
	/** 按到达顺序排列的等待中问题，及工具的回调。 */
	TArray<FShuttlegentQuestion> Pending;
	TMap<int32, TFunction<void(EShuttlegentQuestionOutcome, const FString&)>> Callbacks;

	int32 NextQuestionId = 1;
	bool bAnswererAttached = false;

	/** 检查等待中问题是否超出时限；仅在有问题等待时运行。 */
	FTSTicker::FDelegateHandle TimeoutHandle;

public:
	void Shutdown();

	/** 面板已打开，可回答问题；解除挂接时将仍在等待的问题以 NoPanel 关闭。 */
	void SetAnswererAttached(bool bAttached);

	/** 向用户提问；OnClosed 以结果和答案运行一次。返回问题 id。 */
	int32 Ask(FShuttlegentQuestion Question, TFunction<void(EShuttlegentQuestionOutcome, const FString&)> OnClosedCallback);

	/** 用户的答案或跳过；问题已不在等待时返回 false，因此重复点击不产生作用。 */
	bool Answer(int32 QuestionId, const FString& InAnswer, bool bSkip);

	/** 令所有等待中的问题在未作答状态下过期。 */
	void ExpireAll();

	const TArray<FShuttlegentQuestion>& GetPending() const { return Pending; }

private:
	void Close(int32 QuestionId, EShuttlegentQuestionOutcome Outcome, const FString& InAnswer);
	bool HandleTimeoutCheck(float DeltaTime);
};
