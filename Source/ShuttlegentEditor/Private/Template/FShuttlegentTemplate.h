#pragma once

#include "CoreMinimal.h"

/** One slash template: a Markdown file of a template directory, inserted into the draft from the / menu. */
struct FShuttlegentTemplate
{
	/** File name without extension; what the user types after /. */
	FString Name;

	/** First line of the file without Markdown heading marks, shown in the menu. */
	FString Description;

	/** The whole file, inserted into the draft. */
	FString Body;
};
