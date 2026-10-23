#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

#include "Mark/EShuttlegentMarkMotion.h"
#include "Mark/EShuttlegentMarkTier.h"

class FActiveTimerHandle;
struct FShuttlegentMarkSheet;

/**
 * Shuttlegent mark that plays the exported sprite-sheet animations (version D, "woven picks").
 *
 * Shows the rest frame unless a scene is playing. A timer runs only while a scene plays and
 * one-shot scenes return to rest when they end. Falls back to the static mark brush when
 * the panel's palette under Resources/anim is missing, and always rests when the "Animate Mark" setting is off.
 */
class SShuttlegentMark : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentMark)
		: _Tier(EShuttlegentMarkTier::Small)
		, _bHoverEnabled(true)
		, _ColorAndOpacity(FLinearColor::White)
		{}
		SLATE_ARGUMENT(EShuttlegentMarkTier, Tier)
		SLATE_ARGUMENT(bool, bHoverEnabled)
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
	SLATE_END_ARGS()

private:
	TAttribute<FSlateColor> ColorAndOpacity;
	TSharedPtr<FActiveTimerHandle> ActiveTimer;

	/** One frame of the current sheet; kept on the widget so the draw element can reference it. */
	mutable FSlateBrush FrameBrush {};

	double StartTime = 0.0;
	EShuttlegentMarkTier Tier = EShuttlegentMarkTier::Small;
	EShuttlegentMarkMotion Motion = EShuttlegentMarkMotion::None;
	bool bHoverEnabled = true;

public:
	void Construct(const FArguments& InArgs);

	/** Plays a scene from its first frame; ignored when the tier has no sheet for it or animation is off. */
	void Play(EShuttlegentMarkMotion InMotion);

	/** Stops any scene and shows the rest frame. */
	void ShowRest();

	EShuttlegentMarkMotion GetMotion() const { return Motion; }

	//~ Begin SWidget Interface.
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End SWidget Interface.

protected:
	//~ Begin SWidget Interface.
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	//~ End SWidget Interface.

private:
	/** Sheet whose last frame is the rest frame of this tier. */
	const FShuttlegentMarkSheet* GetRestSheet() const;

	/** Sheet of the playing scene, or nullptr at rest. */
	const FShuttlegentMarkSheet* GetPlayingSheet() const;

	/** Frame of the playing scene at the current Slate time. */
	int32 GetPlayingFrame(const FShuttlegentMarkSheet& Sheet) const;

	bool IsAnimationEnabled() const;
	void StopTimer();
	EActiveTimerReturnType HandleActiveTimer(double InCurrentTime, float InDeltaTime);
};
