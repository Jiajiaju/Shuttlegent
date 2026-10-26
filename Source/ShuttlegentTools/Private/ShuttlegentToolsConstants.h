#pragma once

#include "CoreMinimal.h"

/** Limits shared by the built-in editor tools (Shuttlegent.md section 10.2). */
namespace ShuttlegentToolsConstant
{
	// Longest property value shown per property, in characters; longer values end with an ellipsis
	inline constexpr int32 MaxValueChars = 400;

	// Asset tags shown by get_asset at most
	inline constexpr int32 MaxAssetTags = 40;

	// Log lines kept since editor start for read_log, and how many it returns by default and at most
	inline constexpr int32 LogCapacity = 5000;
	inline constexpr int32 DefaultLogLines = 100;
	inline constexpr int32 MaxLogLines = 500;

	// Undo and redo steps one call takes at most
	inline constexpr int32 MaxHistorySteps = 20;

	// First character of an object or package path; a name without it is an actor label
	inline const TCHAR* PathPrefix = TEXT("/");

	// Default package path searched by find_assets
	inline const TCHAR* DefaultAssetRoot = TEXT("/Game");

	// Class name prefixes a model may write that UE class names omit (AStaticMeshActor -> StaticMeshActor)
	inline const TCHAR* ClassPrefixes[] = { TEXT("A"), TEXT("U") };

	// Suffix of a Blueprint generated class
	inline const TCHAR* GeneratedClassSuffix = TEXT("_C");

	// Shown after a value cut to MaxValueChars
	inline const TCHAR* Ellipsis = TEXT("…");

	// Property metadata key of the Details panel category
	inline const TCHAR* MetaCategory = TEXT("Category");

	// Type shown for bool properties, which C++ declares as uint8 bitfields as often as bool
	inline const TCHAR* BoolTypeName = TEXT("bool");
}
