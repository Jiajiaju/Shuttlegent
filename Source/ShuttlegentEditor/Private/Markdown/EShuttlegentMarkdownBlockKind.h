#pragma once

#include "CoreMinimal.h"

/** Kind of one block of a Markdown message. */
enum class EShuttlegentMarkdownBlockKind : uint8
{
	// Lines of text up to a blank line
	Paragraph,

	// "#" to "######" heading; Level is 1-6
	Heading,

	// "-", "*", "+" or "1." item; Level is the nesting depth from 0
	ListItem,

	// "> " lines
	Quote,

	// Fenced ``` block, shown verbatim
	Code,

	// "|" table lines, shown verbatim in the code font
	Table,

	// "---" line
	Rule
};
