#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;
class ISlateStyle;
struct FSlateBrush;

/** Slate style set of the Shuttlegent panel: the dock tab icon and the brand mark of the loading placeholder. The panel itself is a web page (Resources/Web). */
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
