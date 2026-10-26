#pragma once

#include "CoreMinimal.h"

#include "Template/FShuttlegentTemplate.h"

/** Reads the slash templates of the project's template directories (Shuttlegent.md section 11). */
struct FShuttlegentTemplateLibrary
{
	/** Every *.md file of the directories and their sub-folders, sorted by name; of two files with one name the first directory wins. */
	static TArray<FShuttlegentTemplate> Load();
};
