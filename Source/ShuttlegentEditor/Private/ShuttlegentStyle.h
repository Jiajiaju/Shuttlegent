#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"

class FSlateStyleSet;
class ISlateStyle;
struct FSlateBrush;
struct FShuttlegentMarkSheet;
enum class EShuttlegentMarkMotion : uint8;
enum class EShuttlegentMarkTier : uint8;

/** Slate style set of the Shuttlegent panel, built from the VI 1.1 dark theme and the plugin's Resources. */
class FShuttlegentStyle
{
private:
	static TSharedPtr<FSlateStyleSet> StyleSet;

public:
	static void Register();
	static void Unregister();

	static const ISlateStyle& Get();

	/** Brush of a function icon, e.g. Icon(TEXT("send")). */
	static const FSlateBrush* Icon(const TCHAR* Name);

	static const FSlateBrush* Brush(const FName& Key);

	/** Sprite sheet of a mark animation scene, or nullptr when the tier does not carry it or the panel's palette under Resources/anim is missing. */
	static const FShuttlegentMarkSheet* MarkSheet(EShuttlegentMarkTier Tier, EShuttlegentMarkMotion Motion);

	/** Linear color of a VI token (sRGB FColor converted once). */
	static FLinearColor Color(const FColor& Token);

	/** Slate color of a VI token, for ColorAndOpacity. */
	static FSlateColor Tint(const FColor& Token);

	static FSlateFontInfo BodyFont();
	static FSlateFontInfo CaptionFont();
	static FSlateFontInfo SectionFont();
	static FSlateFontInfo EmphasisFont();

	/** Empty-state title. */
	static FSlateFontInfo DisplayFont();

	/** Monospaced caption: run log times and tool parameters. */
	static FSlateFontInfo MonoCaptionFont();
};
