#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "FShuttlegentModelProbeTarget.h"

class SVerticalBox;
struct FShuttlegentModelProbeResult;

/**
 * 设置页的“检测连接”行：点击后依次检测各个模型，每个模型一行结论；结论之下给出实际请求地址，
 * 失败时附服务方的错误说明。检测期间按钮不可用。
 */
class SShuttlegentProbeRow : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(TArray<FShuttlegentModelProbeTarget>, FOnGetTargets);

	SLATE_BEGIN_ARGS(SShuttlegentProbeRow) {}
		/** 点击时取检测对象，取当前设置值，因此改完设置不用刷新页面。 */
		SLATE_EVENT(FOnGetTargets, OnGetTargets)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FOnGetTargets OnGetTargets;

	/** 尚未检测的对象，按顺序逐个发出。 */
	TArray<FShuttlegentModelProbeTarget> Pending;

	TSharedPtr<SVerticalBox> ResultBox;

	bool bProbing = false;

	FReply HandleClicked();
	void ProbeNext();
	void HandleProbeDone(const FShuttlegentModelProbeResult& Result);
	void AddResultLines(const FShuttlegentModelProbeResult& Result);
};
