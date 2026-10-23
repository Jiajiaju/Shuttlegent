#include "Markdown/FShuttlegentMarkdown.h"

#include "Markdown/FShuttlegentMarkdownBlock.h"
#include "ShuttlegentEditorConstants.h"

namespace FShuttlegentMarkdownPrivate
{
	// Block syntax
	inline const TCHAR* FenceBacktick = TEXT("```");
	inline const TCHAR* FenceTilde = TEXT("~~~");
	inline constexpr TCHAR QuoteMark = TEXT('>');
	inline constexpr TCHAR TableMark = TEXT('|');
	inline constexpr TCHAR HeadingMark = TEXT('#');
	inline constexpr int32 MaxHeadingLevel = 6;
	inline constexpr int32 MinRuleMarks = 3;

	// Lists: bullet shown for unordered items, marker characters, spaces per nesting level, deepest level, tab width
	inline const TCHAR* Bullet = TEXT("•");
	inline const TCHAR* OrderedSuffix = TEXT(".");
	inline constexpr TCHAR Dash = TEXT('-');
	inline constexpr TCHAR Plus = TEXT('+');
	inline constexpr TCHAR Dot = TEXT('.');
	inline constexpr TCHAR CloseParen = TEXT(')');
	inline constexpr TCHAR Space = TEXT(' ');
	inline constexpr TCHAR Tab = TEXT('\t');
	inline constexpr int32 SpacesPerListLevel = 2;
	inline constexpr int32 MaxListLevel = 4;
	inline constexpr int32 TabWidth = 4;

	// Joins the lines of one block
	inline const TCHAR* LineBreak = TEXT("\n");

	// Inline syntax
	inline const TCHAR* BoldStars = TEXT("**");
	inline const TCHAR* BoldUnderscores = TEXT("__");
	inline const TCHAR* LinkMiddle = TEXT("](");
	inline const TCHAR* LinkEnd = TEXT(")");
	inline constexpr TCHAR CodeMark = TEXT('`');
	inline constexpr TCHAR Star = TEXT('*');
	inline constexpr TCHAR Underscore = TEXT('_');
	inline constexpr TCHAR LinkStart = TEXT('[');
	inline constexpr int32 BoldMarkLength = 2;
	inline constexpr int32 LinkMiddleLength = 2;

	// Rich-text markup
	inline const TCHAR* TagOpen = TEXT("<");
	inline const TCHAR* TagOpenEnd = TEXT(">");
	inline const TCHAR* TagEnd = TEXT("</>");
	inline const TCHAR* LinkTagFormat = TEXT("<a id=\"{0}\" href=\"{1}\" style=\"{2}\">");

	// Characters the default rich-text parser unescapes (RichTextMarkupProcessing.cpp)
	inline constexpr TCHAR Ampersand = TEXT('&');
	inline constexpr TCHAR DoubleQuote = TEXT('"');
	inline constexpr TCHAR Less = TEXT('<');
	inline constexpr TCHAR Greater = TEXT('>');
	inline const TCHAR* EscapeAmpersand = TEXT("&amp;");
	inline const TCHAR* EscapeQuote = TEXT("&quot;");
	inline const TCHAR* EscapeLess = TEXT("&lt;");
	inline const TCHAR* EscapeGreater = TEXT("&gt;");

	void AppendEscaped(FString& Out, const FString& Text)
	{
		for (const TCHAR Char : Text)
		{
			if (Char == Ampersand) Out += EscapeAmpersand;
			else if (Char == DoubleQuote) Out += EscapeQuote;
			else if (Char == Less) Out += EscapeLess;
			else if (Char == Greater) Out += EscapeGreater;
			else Out.AppendChar(Char);
		}
	}

	void AppendStyled(FString& Out, const TCHAR* Style, const FString& Text)
	{
		Out += TagOpen;
		Out += Style;
		Out += TagOpenEnd;
		AppendEscaped(Out, Text);
		Out += TagEnd;
	}

	bool IsWordChar(TCHAR Char)
	{
		return FChar::IsAlnum(Char) || Char == Underscore;
	}

	bool IsFence(const FString& Trimmed)
	{
		return Trimmed.StartsWith(FenceBacktick) || Trimmed.StartsWith(FenceTilde);
	}

	bool IsRule(const FString& Trimmed)
	{
		if (Trimmed.Len() < MinRuleMarks) return false;

		const TCHAR First = Trimmed[0];
		if (First != Dash && First != Star && First != Underscore) return false;

		for (const TCHAR Char : Trimmed)
		{
			if (Char != First) return false;
		}
		return true;
	}

	/** "## Title" -> level 2 and "Title"; false when the line is not a heading. */
	bool ParseHeading(const FString& Trimmed, int32& OutLevel, FString& OutText)
	{
		int32 Level = 0;
		while (Level < Trimmed.Len() && Trimmed[Level] == HeadingMark)
		{
			++Level;
		}
		if (Level == 0 || Level > MaxHeadingLevel || Level >= Trimmed.Len() || !FChar::IsWhitespace(Trimmed[Level])) return false;

		OutLevel = Level;
		OutText = Trimmed.Mid(Level).TrimStartAndEnd();
		return true;
	}

	int32 LeadingSpaces(const FString& Line)
	{
		int32 Spaces = 0;
		for (const TCHAR Char : Line)
		{
			if (Char == Space) ++Spaces;
			else if (Char == Tab) Spaces += TabWidth;
			else break;
		}
		return Spaces;
	}

	/** "  - item" / "3. item" -> depth, marker as shown and item text; false when the line is not a list item. */
	bool ParseListItem(const FString& Line, int32& OutDepth, FString& OutMarker, FString& OutText)
	{
		const FString Rest = Line.TrimStart();
		if (Rest.Len() < 2) return false;

		const TCHAR First = Rest[0];
		if ((First == Dash || First == Star || First == Plus) && FChar::IsWhitespace(Rest[1]))
		{
			OutMarker = Bullet;
			OutText = Rest.Mid(2).TrimStartAndEnd();
		}
		else
		{
			int32 Digits = 0;
			while (Digits < Rest.Len() && FChar::IsDigit(Rest[Digits]))
			{
				++Digits;
			}

			const bool bOrdered = Digits > 0 && Digits + 1 < Rest.Len() && (Rest[Digits] == Dot || Rest[Digits] == CloseParen) && FChar::IsWhitespace(Rest[Digits + 1]);
			if (!bOrdered) return false;

			OutMarker = Rest.Left(Digits) + OrderedSuffix;
			OutText = Rest.Mid(Digits + 2).TrimStartAndEnd();
		}

		OutDepth = FMath::Min(LeadingSpaces(Line) / SpacesPerListLevel, MaxListLevel);
		return true;
	}

	/** A line that starts its own block, so it does not continue a list item. */
	bool StartsBlock(const FString& Line)
	{
		const FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty()) return false;

		int32 Level = 0;
		FString Text;
		int32 Depth = 0;
		FString Marker;
		return IsFence(Trimmed) || IsRule(Trimmed) || ParseHeading(Trimmed, Level, Text) || ParseListItem(Line, Depth, Marker, Text) || Trimmed[0] == QuoteMark || Trimmed[0] == TableMark;
	}

	FString InlineToMarkup(const FString& Text)
	{
		FString Out;
		int32 Index = 0;
		const int32 Length = Text.Len();
		while (Index < Length)
		{
			const TCHAR Char = Text[Index];

			// `code`
			if (Char == CodeMark)
			{
				int32 End = INDEX_NONE;
				const bool bClosed = Text.Mid(Index + 1).FindChar(CodeMark, End);
				if (bClosed)
				{
					AppendStyled(Out, ShuttlegentEditorStyleKey::MarkdownCode, Text.Mid(Index + 1, End));
					Index += End + 2;
					continue;
				}
			}

			// **bold** or __bold__
			const FString Pair = Text.Mid(Index, BoldMarkLength);
			const TCHAR* BoldMark = Pair == BoldStars ? BoldStars : (Pair == BoldUnderscores ? BoldUnderscores : nullptr);
			if (BoldMark != nullptr)
			{
				const int32 End = Text.Find(BoldMark, ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + BoldMarkLength);
				if (End > Index + BoldMarkLength)
				{
					AppendStyled(Out, ShuttlegentEditorStyleKey::MarkdownBold, Text.Mid(Index + BoldMarkLength, End - Index - BoldMarkLength));
					Index = End + BoldMarkLength;
					continue;
				}
			}

			// *italic* or _italic_ (an underscore inside a word stays literal)
			const bool bItalicOpen = (Char == Star || (Char == Underscore && (Index == 0 || !IsWordChar(Text[Index - 1])))) && Index + 1 < Length && !FChar::IsWhitespace(Text[Index + 1]);
			if (bItalicOpen)
			{
				int32 Offset = INDEX_NONE;
				const bool bFound = Text.Mid(Index + 1).FindChar(Char, Offset);
				const int32 End = bFound ? Index + 1 + Offset : INDEX_NONE;
				const bool bClosed = bFound && Offset > 0 && (Char == Star || End + 1 >= Length || !IsWordChar(Text[End + 1]));
				if (bClosed)
				{
					AppendStyled(Out, ShuttlegentEditorStyleKey::MarkdownItalic, Text.Mid(Index + 1, Offset));
					Index = End + 1;
					continue;
				}
			}

			// [text](url)
			if (Char == LinkStart)
			{
				const int32 Middle = Text.Find(LinkMiddle, ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + 1);
				const int32 End = Middle != INDEX_NONE ? Text.Find(LinkEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, Middle + LinkMiddleLength) : INDEX_NONE;
				if (End != INDEX_NONE)
				{
					FString Url;
					AppendEscaped(Url, Text.Mid(Middle + LinkMiddleLength, End - Middle - LinkMiddleLength));
					Out += FString::Format(LinkTagFormat, { ShuttlegentEditorStyleKey::MarkdownLinkId, Url, ShuttlegentEditorStyleKey::MarkdownLink });
					AppendEscaped(Out, Text.Mid(Index + 1, Middle - Index - 1));
					Out += TagEnd;
					Index = End + 1;
					continue;
				}
			}

			AppendEscaped(Out, FString(1, &Char));
			++Index;
		}
		return Out;
	}

	void AddMarkupBlock(TArray<FShuttlegentMarkdownBlock>& Blocks, EShuttlegentMarkdownBlockKind Kind, const FString& Text)
	{
		FShuttlegentMarkdownBlock& Block = Blocks.AddDefaulted_GetRef();
		Block.Kind = Kind;
		Block.Markup = InlineToMarkup(Text);
	}

	TArray<FShuttlegentMarkdownBlock> Parse(const FString& Source)
	{
		TArray<FShuttlegentMarkdownBlock> Blocks;
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);

		TArray<FString> Paragraph;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			const FString& Line = Lines[Index];
			const FString Trimmed = Line.TrimStartAndEnd();

			// Any non-paragraph line closes the open paragraph
			const bool bParagraphLine = !Trimmed.IsEmpty() && !StartsBlock(Line);
			if (!bParagraphLine && !Paragraph.IsEmpty())
			{
				AddMarkupBlock(Blocks, EShuttlegentMarkdownBlockKind::Paragraph, FString::Join(Paragraph, LineBreak));
				Paragraph.Reset();
			}

			if (bParagraphLine)
			{
				Paragraph.Add(Line.TrimEnd());
				continue;
			}
			if (Trimmed.IsEmpty()) continue;

			if (IsFence(Trimmed))
			{
				// An unclosed fence (reply still streaming) runs to the end
				TArray<FString> CodeLines;
				int32 CloseIndex = Index + 1;
				while (CloseIndex < Lines.Num() && !IsFence(Lines[CloseIndex].TrimStartAndEnd()))
				{
					CodeLines.Add(Lines[CloseIndex]);
					++CloseIndex;
				}

				FShuttlegentMarkdownBlock& Block = Blocks.AddDefaulted_GetRef();
				Block.Kind = EShuttlegentMarkdownBlockKind::Code;
				Block.Language = Trimmed.Mid(FCString::Strlen(FenceBacktick)).TrimStartAndEnd();
				Block.Verbatim = FString::Join(CodeLines, LineBreak);
				Index = CloseIndex;
				continue;
			}

			int32 HeadingLevel = 0;
			FString HeadingText;
			if (ParseHeading(Trimmed, HeadingLevel, HeadingText))
			{
				AddMarkupBlock(Blocks, EShuttlegentMarkdownBlockKind::Heading, HeadingText);
				Blocks.Last().Level = HeadingLevel;
				continue;
			}

			if (IsRule(Trimmed))
			{
				Blocks.AddDefaulted_GetRef().Kind = EShuttlegentMarkdownBlockKind::Rule;
				continue;
			}

			if (Trimmed[0] == QuoteMark || Trimmed[0] == TableMark)
			{
				const TCHAR Mark = Trimmed[0];
				TArray<FString> BlockLines;
				while (Index < Lines.Num())
				{
					const FString Current = Lines[Index].TrimStartAndEnd();
					if (Current.IsEmpty() || Current[0] != Mark) break;

					BlockLines.Add(Mark == QuoteMark ? Current.Mid(1).TrimStart() : Current);
					++Index;
				}
				--Index;

				if (Mark == QuoteMark)
				{
					AddMarkupBlock(Blocks, EShuttlegentMarkdownBlockKind::Quote, FString::Join(BlockLines, LineBreak));
				}
				else
				{
					FShuttlegentMarkdownBlock& Block = Blocks.AddDefaulted_GetRef();
					Block.Kind = EShuttlegentMarkdownBlockKind::Table;
					Block.Verbatim = FString::Join(BlockLines, LineBreak);
				}
				continue;
			}

			int32 Depth = 0;
			FString Marker;
			FString ItemText;
			if (ParseListItem(Line, Depth, Marker, ItemText))
			{
				// Indented lines that start no block of their own continue the item
				while (Index + 1 < Lines.Num() && LeadingSpaces(Lines[Index + 1]) > 0 && !Lines[Index + 1].TrimStartAndEnd().IsEmpty() && !StartsBlock(Lines[Index + 1]))
				{
					++Index;
					ItemText += LineBreak + Lines[Index].TrimStartAndEnd();
				}

				AddMarkupBlock(Blocks, EShuttlegentMarkdownBlockKind::ListItem, ItemText);
				Blocks.Last().Level = Depth;
				Blocks.Last().Marker = Marker;
			}
		}

		if (!Paragraph.IsEmpty()) AddMarkupBlock(Blocks, EShuttlegentMarkdownBlockKind::Paragraph, FString::Join(Paragraph, LineBreak));
		return Blocks;
	}
}

TArray<FShuttlegentMarkdownBlock> FShuttlegentMarkdown::Parse(const FString& Source)
{
	return FShuttlegentMarkdownPrivate::Parse(Source);
}

FString FShuttlegentMarkdown::InlineToMarkup(const FString& Text)
{
	return FShuttlegentMarkdownPrivate::InlineToMarkup(Text);
}
