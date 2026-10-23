#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
struct FShuttlegentChatEntry;
struct FShuttlegentMarkdownBlock;

/**
 * Markdown body of an assistant message. Re-parses and rebuilds when the entry's text grows while a reply streams.
 * Text blocks are read-only rich text that can be selected and copied; they take no IME input, so the
 * editor-tool-dev ban on multi-line Slate input controls does not apply. Only http(s) links open.
 */
class SShuttlegentMarkdownView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentMarkdownView) {}
		SLATE_ARGUMENT(TWeakPtr<FShuttlegentChatEntry>, Entry)
	SLATE_END_ARGS()

private:
	TWeakPtr<FShuttlegentChatEntry> Entry;
	TSharedPtr<SVerticalBox> BlockList;

	/** Length of the text the blocks were built from; replies only grow while streaming. */
	int32 ShownLength = INDEX_NONE;

public:
	void Construct(const FArguments& InArgs);

	//~ Begin SWidget Interface.
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget Interface.

private:
	void Rebuild();
	TSharedRef<SWidget> MakeBlock(const FShuttlegentMarkdownBlock& Block) const;
	TSharedRef<SWidget> MakeRichText(const FString& Markup, const TCHAR* DefaultStyle) const;
	TSharedRef<SWidget> MakeVerbatim(const FString& Text, const FString& Label) const;
};
