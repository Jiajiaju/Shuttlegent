#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 设置页一条模型连接的检测行：一个图标按钮，旁边一个圆点与一个词表示结论（Untested / Testing / Connected / Failed）。
 * 结论读自设置中该连接的状态，地址或密钥改动后的自动检测也会反映在这里；详细过程只写 Output Log。
 */
class SShuttlegentProbeRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentProbeRow) {}
		/** 连接在 UShuttlegentSettings::Connections 中的下标；数组增删后跟着变，所以按属性取。 */
		SLATE_ATTRIBUTE(int32, ConnectionIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TAttribute<int32> ConnectionIndex;

	FReply HandleClicked() const;
	bool IsTesting() const;
	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
};
