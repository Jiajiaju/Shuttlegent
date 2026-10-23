#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

/** One sprite sheet of the mark animation, as described by Resources/anim/<palette>/mark-anim.json. */
struct FShuttlegentMarkSheet
{
	/** Whole sheet; its UV region is narrowed to one frame when painted. */
	FSlateBrush Brush {};

	/** Size of one frame in the sheet, in pixels. */
	FVector2f FrameSize {};

	/** Size of the whole sheet, in pixels. */
	FVector2f SheetSize {};

	/** Transparent gutter between frames, in pixels. */
	float Padding = 0.0f;

	int32 FrameCount = 0;
	int32 Columns = 0;
	int32 Fps = 0;

	/** Working loops; the other scenes play once and hold their last frame. */
	bool bLoop = false;
};
