#include "FShuttlegentMemoryStore.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"

#define LOCTEXT_NAMESPACE "ShuttlegentMemoryStore"

namespace FShuttlegentMemoryStorePrivate
{
	// 记录日期的字符数（YYYY-MM-DD）
	inline constexpr int32 DateChars = 10;

	FString Today()
	{
		return FDateTime::Now().ToString(ShuttlegentMemoryConstant::DateFormat);
	}

	/** 一条记忆只占一行：换行并为空格，首尾空白去掉。 */
	FString NormalizeText(const FString& Text)
	{
		FString Result = Text.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
		return Result.TrimStartAndEnd();
	}

	bool ParseItem(const FString& Line, FShuttlegentMemoryItem& OutItem)
	{
		if (!Line.StartsWith(ShuttlegentMemoryConstant::ItemPrefix)) return false;

		FString Body = Line.RightChop(FCString::Strlen(ShuttlegentMemoryConstant::ItemPrefix)).TrimStartAndEnd();
		OutItem = FShuttlegentMemoryItem();

		// 末尾的“（YYYY-MM-DD）”是记录日期；手工写的行可能没有
		const int32 OpenIndex = Body.Find(ShuttlegentMemoryConstant::DateOpen, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		const bool bDated = OpenIndex != INDEX_NONE && Body.EndsWith(ShuttlegentMemoryConstant::DateClose) && Body.Len() - OpenIndex - 2 == DateChars;
		if (bDated)
		{
			OutItem.Date = Body.Mid(OpenIndex + 1, DateChars);
			Body = Body.Left(OpenIndex).TrimEnd();
		}

		OutItem.Text = Body;
		return !Body.IsEmpty();
	}

	FString FormatItem(const FShuttlegentMemoryItem& Item)
	{
		FString Line = FString(ShuttlegentMemoryConstant::ItemPrefix) + Item.Text;
		if (!Item.Date.IsEmpty()) Line += FString(ShuttlegentMemoryConstant::DateOpen) + Item.Date + ShuttlegentMemoryConstant::DateClose;
		return Line;
	}

	TArray<FString> ReadLines()
	{
		FString Content;
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToString(Content, *FShuttlegentMemoryStore::GetMemoryFile())) return Lines;

		Content.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Content.ParseIntoArray(Lines, TEXT("\n"), false);
		while (!Lines.IsEmpty() && Lines.Last().TrimStartAndEnd().IsEmpty())
		{
			Lines.Pop();
		}
		return Lines;
	}

	/** 与 Text 内容相同的那一条所在的行；没有时为 INDEX_NONE。 */
	int32 FindItem(const TArray<FString>& Lines, const FString& Text)
	{
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FShuttlegentMemoryItem Item;
			if (ParseItem(Lines[LineIndex], Item) && Item.Text == Text) return LineIndex;
		}
		return INDEX_NONE;
	}

	/** 写回文件；超出上限时不写并给出原因。 */
	bool WriteLines(const TArray<FString>& Lines, FString& OutError)
	{
		const FString Content = FString::Join(Lines, TEXT("\n")) + TEXT("\n");
		const int32 Bytes = FTCHARToUTF8(*Content).Length();
		if (Bytes > ShuttlegentMemoryConstant::MaxFileBytes)
		{
			OutError = FString::Printf(TEXT("Memory is full (%d of %d bytes). Merge related lines (remember with replaces) or forget lines that no longer apply, then try again."), Bytes, ShuttlegentMemoryConstant::MaxFileBytes);
			return false;
		}

		IFileManager::Get().MakeDirectory(*FShuttlegentMemoryStore::GetLocalDir(), true);
		if (!FFileHelper::SaveStringToFile(Content, *FShuttlegentMemoryStore::GetMemoryFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("Could not write %s."), *FShuttlegentMemoryStore::GetMemoryFile());
			return false;
		}
		return true;
	}

	void AppendItem(TArray<FString>& Lines, const FShuttlegentMemoryItem& Item)
	{
		if (Lines.IsEmpty())
		{
			Lines.Add(ShuttlegentMemoryConstant::FileHeading);
			Lines.Add(FString());
		}
		Lines.Add(FormatItem(Item));
	}
}

FOnShuttlegentMemoryChanged& FShuttlegentMemoryStore::OnChanged()
{
	static FOnShuttlegentMemoryChanged Event;
	return Event;
}

FString FShuttlegentMemoryStore::GetLocalDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), ShuttlegentMemoryConstant::LocalDir));
}

FString FShuttlegentMemoryStore::GetMemoryFile()
{
	return FPaths::Combine(GetLocalDir(), ShuttlegentMemoryConstant::MemoryFile);
}

TArray<FShuttlegentMemoryItem> FShuttlegentMemoryStore::Load()
{
	TArray<FShuttlegentMemoryItem> Items;
	for (const FString& Line : FShuttlegentMemoryStorePrivate::ReadLines())
	{
		FShuttlegentMemoryItem Item;
		if (FShuttlegentMemoryStorePrivate::ParseItem(Line, Item)) Items.Add(MoveTemp(Item));
	}
	return Items;
}

int32 FShuttlegentMemoryStore::GetFileBytes()
{
	const int64 Size = IFileManager::Get().FileSize(*GetMemoryFile());
	return Size > 0 ? static_cast<int32>(Size) : 0;
}

bool FShuttlegentMemoryStore::Remember(const FString& Text, const FString& Replaces, bool bByAgent, bool& OutChanged, FString& OutError)
{
	OutChanged = false;
	const FString Normalized = FShuttlegentMemoryStorePrivate::NormalizeText(Text);
	const FString Replaced = FShuttlegentMemoryStorePrivate::NormalizeText(Replaces);
	if (Normalized.IsEmpty())
	{
		OutError = TEXT("Pass the fact to remember.");
		return false;
	}
	if (Normalized.Len() > ShuttlegentMemoryConstant::MaxItemChars)
	{
		OutError = FString::Printf(TEXT("Keep one fact in one short sentence (at most %d characters)."), ShuttlegentMemoryConstant::MaxItemChars);
		return false;
	}

	TArray<FString> Lines = FShuttlegentMemoryStorePrivate::ReadLines();
	if (FShuttlegentMemoryStorePrivate::FindItem(Lines, Normalized) != INDEX_NONE && (Replaced.IsEmpty() || Replaced == Normalized)) return true;

	FShuttlegentMemoryChange Change;
	Change.After.Text = Normalized;
	Change.After.Date = FShuttlegentMemoryStorePrivate::Today();
	if (Replaced.IsEmpty())
	{
		Change.Kind = EShuttlegentMemoryChangeKind::Added;
		FShuttlegentMemoryStorePrivate::AppendItem(Lines, Change.After);
	}
	else
	{
		const int32 LineIndex = FShuttlegentMemoryStorePrivate::FindItem(Lines, Replaced);
		if (LineIndex == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("No remembered line reads exactly \"%s\". Pass the line as it appears in your memory, or leave replaces empty to add a new line."), *Replaced);
			return false;
		}

		Change.Kind = EShuttlegentMemoryChangeKind::Updated;
		FShuttlegentMemoryStorePrivate::ParseItem(Lines[LineIndex], Change.Before);
		Lines[LineIndex] = FShuttlegentMemoryStorePrivate::FormatItem(Change.After);
	}

	if (!FShuttlegentMemoryStorePrivate::WriteLines(Lines, OutError)) return false;

	OutChanged = true;
	UE_LOG(LogShuttlegent, Log, TEXT("Memory %s: %s"), Change.Kind == EShuttlegentMemoryChangeKind::Added ? TEXT("added") : TEXT("updated"), *Normalized);
	OnChanged().Broadcast(Change, bByAgent);
	return true;
}

bool FShuttlegentMemoryStore::Forget(const FString& Text, bool bByAgent, FString& OutError)
{
	const FString Normalized = FShuttlegentMemoryStorePrivate::NormalizeText(Text);
	TArray<FString> Lines = FShuttlegentMemoryStorePrivate::ReadLines();
	const int32 LineIndex = FShuttlegentMemoryStorePrivate::FindItem(Lines, Normalized);
	if (LineIndex == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("No remembered line reads exactly \"%s\". Pass the line as it appears in your memory."), *Normalized);
		return false;
	}

	FShuttlegentMemoryChange Change;
	Change.Kind = EShuttlegentMemoryChangeKind::Removed;
	FShuttlegentMemoryStorePrivate::ParseItem(Lines[LineIndex], Change.Before);
	Lines.RemoveAt(LineIndex);
	if (!FShuttlegentMemoryStorePrivate::WriteLines(Lines, OutError)) return false;

	UE_LOG(LogShuttlegent, Log, TEXT("Memory removed: %s"), *Normalized);
	OnChanged().Broadcast(Change, bByAgent);
	return true;
}

bool FShuttlegentMemoryStore::Revert(const FShuttlegentMemoryChange& Change, FString& OutError)
{
	TArray<FString> Lines = FShuttlegentMemoryStorePrivate::ReadLines();

	// 撤销本身也是一次修改，按相反的方向广播，供管理卡片刷新
	FShuttlegentMemoryChange Inverse;
	Inverse.Before = Change.After;
	Inverse.After = Change.Before;
	switch (Change.Kind)
	{
	case EShuttlegentMemoryChangeKind::Added:
		{
			const int32 LineIndex = FShuttlegentMemoryStorePrivate::FindItem(Lines, Change.After.Text);
			if (LineIndex == INDEX_NONE)
			{
				OutError = LOCTEXT("MemoryGone", "This memory is no longer in the file.").ToString();
				return false;
			}
			Lines.RemoveAt(LineIndex);
			Inverse.Kind = EShuttlegentMemoryChangeKind::Removed;
			break;
		}
	case EShuttlegentMemoryChangeKind::Updated:
		{
			const int32 LineIndex = FShuttlegentMemoryStorePrivate::FindItem(Lines, Change.After.Text);
			if (LineIndex == INDEX_NONE)
			{
				OutError = LOCTEXT("MemoryChanged", "This memory has been changed or removed.").ToString();
				return false;
			}
			Lines[LineIndex] = FShuttlegentMemoryStorePrivate::FormatItem(Change.Before);
			Inverse.Kind = EShuttlegentMemoryChangeKind::Updated;
			break;
		}
	case EShuttlegentMemoryChangeKind::Removed:
	default:
		{
			if (FShuttlegentMemoryStorePrivate::FindItem(Lines, Change.Before.Text) != INDEX_NONE) return true;

			FShuttlegentMemoryStorePrivate::AppendItem(Lines, Change.Before);
			Inverse.Kind = EShuttlegentMemoryChangeKind::Added;
			break;
		}
	}

	if (!FShuttlegentMemoryStorePrivate::WriteLines(Lines, OutError)) return false;

	UE_LOG(LogShuttlegent, Log, TEXT("Memory change reverted: %s"), *(Change.After.Text.IsEmpty() ? Change.Before.Text : Change.After.Text));
	OnChanged().Broadcast(Inverse, false);
	return true;
}

void FShuttlegentMemoryStore::MigrateLocalPrompt()
{
	const FString OldPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), ShuttlegentMemoryConstant::LocalPromptFile));
	const FString NewPath = FPaths::Combine(GetLocalDir(), ShuttlegentMemoryConstant::LocalPromptFile);
	if (!FPaths::FileExists(OldPath) || FPaths::FileExists(NewPath)) return;

	IFileManager::Get().MakeDirectory(*GetLocalDir(), true);
	if (IFileManager::Get().Move(*NewPath, *OldPath))
	{
		UE_LOG(LogShuttlegent, Display, TEXT("Moved the personal prompt %s to %s (personal files now live in Saved/Shuttlegent)."), *OldPath, *NewPath);
		return;
	}
	UE_LOG(LogShuttlegent, Warning, TEXT("Could not move the personal prompt %s to %s; it is no longer loaded from the project root."), *OldPath, *NewPath);
}

#undef LOCTEXT_NAMESPACE
