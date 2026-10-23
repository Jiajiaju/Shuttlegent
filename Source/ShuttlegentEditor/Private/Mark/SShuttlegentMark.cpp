#include "Mark/SShuttlegentMark.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

#include "Mark/FShuttlegentMarkSheet.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentSettings.h"
#include "ShuttlegentStyle.h"

namespace SShuttlegentMarkPrivate
{
	/** Scene whose last frame is the rest frame: both are pixel-identical to the master mark. */
	EShuttlegentMarkMotion RestMotion(EShuttlegentMarkTier Tier)
	{
		return Tier == EShuttlegentMarkTier::Large ? EShuttlegentMarkMotion::Intro : EShuttlegentMarkMotion::Done;
	}

	/** UV rectangle of one frame in a sheet. */
	FBox2f FrameUV(const FShuttlegentMarkSheet& Sheet, int32 Frame)
	{
		const FVector2f Cell = Sheet.FrameSize + FVector2f(Sheet.Padding, Sheet.Padding);
		const FVector2f Origin(static_cast<float>(Frame % Sheet.Columns) * Cell.X, static_cast<float>(Frame / Sheet.Columns) * Cell.Y);
		return FBox2f(Origin / Sheet.SheetSize, (Origin + Sheet.FrameSize) / Sheet.SheetSize);
	}
}

void SShuttlegentMark::Construct(const FArguments& InArgs)
{
	Tier = InArgs._Tier;
	bHoverEnabled = InArgs._bHoverEnabled;
	ColorAndOpacity = InArgs._ColorAndOpacity;
}

void SShuttlegentMark::Play(EShuttlegentMarkMotion InMotion)
{
	const FShuttlegentMarkSheet* Sheet = FShuttlegentStyle::MarkSheet(Tier, InMotion);
	if (Sheet == nullptr || !IsAnimationEnabled())
	{
		ShowRest();
		return;
	}

	Motion = InMotion;
	StartTime = FSlateApplication::Get().GetCurrentTime();

	StopTimer();
	ActiveTimer = RegisterActiveTimer(1.0f / static_cast<float>(Sheet->Fps), FWidgetActiveTimerDelegate::CreateSP(this, &SShuttlegentMark::HandleActiveTimer));
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SShuttlegentMark::ShowRest()
{
	Motion = EShuttlegentMarkMotion::None;
	StopTimer();
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SShuttlegentMark::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const ESlateDrawEffect DrawEffect = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get().GetColor(InWidgetStyle);

	const FShuttlegentMarkSheet* Sheet = GetPlayingSheet();
	int32 Frame = Sheet != nullptr ? GetPlayingFrame(*Sheet) : 0;
	if (Sheet == nullptr)
	{
		Sheet = GetRestSheet();
		Frame = Sheet != nullptr ? Sheet->FrameCount - 1 : 0;
	}

	if (Sheet == nullptr)
	{
		const FSlateBrush* StaticMark = FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Mark);
		if (StaticMark == nullptr) return LayerId;

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), StaticMark, DrawEffect, Tint * StaticMark->GetTint(InWidgetStyle));
		return LayerId;
	}

	FrameBrush = Sheet->Brush;
	FrameBrush.SetUVRegion(SShuttlegentMarkPrivate::FrameUV(*Sheet, Frame));
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &FrameBrush, DrawEffect, Tint);
	return LayerId;
}

void SShuttlegentMark::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SLeafWidget::OnMouseEnter(MyGeometry, MouseEvent);

	if (bHoverEnabled && Motion == EShuttlegentMarkMotion::None) Play(EShuttlegentMarkMotion::Hover);
}

FVector2D SShuttlegentMark::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	const FShuttlegentMarkSheet* Sheet = GetRestSheet();
	if (Sheet != nullptr) return FVector2D(Sheet->FrameSize * ShuttlegentEditorMarkAnim::FrameScale);

	const FSlateBrush* StaticMark = FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Mark);
	if (StaticMark == nullptr) return FVector2D::ZeroVector;

	return FVector2D(StaticMark->ImageSize);
}

const FShuttlegentMarkSheet* SShuttlegentMark::GetRestSheet() const
{
	return FShuttlegentStyle::MarkSheet(Tier, SShuttlegentMarkPrivate::RestMotion(Tier));
}

const FShuttlegentMarkSheet* SShuttlegentMark::GetPlayingSheet() const
{
	if (Motion == EShuttlegentMarkMotion::None) return nullptr;

	return FShuttlegentStyle::MarkSheet(Tier, Motion);
}

int32 SShuttlegentMark::GetPlayingFrame(const FShuttlegentMarkSheet& Sheet) const
{
	const double Elapsed = FMath::Max(0.0, FSlateApplication::Get().GetCurrentTime() - StartTime);
	const int32 Frame = FMath::FloorToInt32(Elapsed * Sheet.Fps);
	return Sheet.bLoop ? Frame % Sheet.FrameCount : FMath::Min(Frame, Sheet.FrameCount - 1);
}

bool SShuttlegentMark::IsAnimationEnabled() const
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	return IsValid(Settings) && Settings->bAnimateMark;
}

void SShuttlegentMark::StopTimer()
{
	if (ActiveTimer.IsValid()) UnRegisterActiveTimer(ActiveTimer.ToSharedRef());
	ActiveTimer.Reset();
}

EActiveTimerReturnType SShuttlegentMark::HandleActiveTimer(double InCurrentTime, float InDeltaTime)
{
	Invalidate(EInvalidateWidgetReason::Paint);

	const FShuttlegentMarkSheet* Sheet = GetPlayingSheet();
	const bool bKeepPlaying = Sheet != nullptr && IsAnimationEnabled() && (Sheet->bLoop || InCurrentTime - StartTime < static_cast<double>(Sheet->FrameCount) / Sheet->Fps);
	if (bKeepPlaying) return EActiveTimerReturnType::Continue;

	Motion = EShuttlegentMarkMotion::None;
	ActiveTimer.Reset();
	return EActiveTimerReturnType::Stop;
}
