#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;
class ISlateStyle;
struct FSlateBrush;

/** Shuttlegent 面板的 Slate 样式集：停靠标签页图标与加载占位的品牌标志。面板本身是网页（Resources/Web）。 */
class FShuttlegentStyle
{
private:
	static TSharedPtr<FSlateStyleSet> StyleSet;

public:
	static void Register();
	static void Unregister();

	static const ISlateStyle& Get();

	static const FSlateBrush* Brush(const FName& Key);
};
