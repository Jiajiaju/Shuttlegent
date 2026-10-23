#pragma once

#include "CoreMinimal.h"

#include "Markdown/EShuttlegentMarkdownBlockKind.h"

/** One parsed block of a Markdown message. */
struct FShuttlegentMarkdownBlock
{
	EShuttlegentMarkdownBlockKind Kind = EShuttlegentMarkdownBlockKind::Paragraph;

	/** Heading level (1-6) or list nesting depth (from 0). */
	int32 Level = 0;

	/** List marker as shown: a bullet or "3.". */
	FString Marker;

	/** Inline content as Slate rich-text markup (paragraph, heading, list item, quote). */
	FString Markup;

	/** Verbatim text (code and table blocks). */
	FString Verbatim;

	/** Language after the opening fence of a code block; may be empty. */
	FString Language;
};
