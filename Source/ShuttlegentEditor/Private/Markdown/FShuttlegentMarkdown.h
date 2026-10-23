#pragma once

#include "CoreMinimal.h"

struct FShuttlegentMarkdownBlock;

/**
 * Minimal Markdown reader for chat messages: headings, lists, quotes, fenced code, tables (verbatim),
 * rules and paragraphs; inline bold, italic, code and links become Slate rich-text markup using the
 * ShuttlegentEditorStyleKey::Markdown* text styles. Tolerates partial input while a reply streams in.
 */
struct FShuttlegentMarkdown
{
	static TArray<FShuttlegentMarkdownBlock> Parse(const FString& Source);

	/** Inline Markdown of one block to rich-text markup; plain text is escaped. */
	static FString InlineToMarkup(const FString& Text);
};
