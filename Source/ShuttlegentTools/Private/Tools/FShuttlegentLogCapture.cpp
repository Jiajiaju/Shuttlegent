#include "Tools/FShuttlegentLogCapture.h"

#include "Algo/Reverse.h"
#include "Misc/ScopeLock.h"

#include "ShuttlegentToolsConstants.h"

FShuttlegentLogCapture::FShuttlegentLogCapture()
{
	Lines.Reserve(ShuttlegentToolsConstant::LogCapacity);
}

TArray<FShuttlegentLogLine> FShuttlegentLogCapture::GetLatest(int32 Count, ELogVerbosity::Type MaxVerbosity, const FString& Category) const
{
	FScopeLock ScopeLock(&Lock);

	// Newest first while collecting, then reversed to read in time order
	TArray<FShuttlegentLogLine> Latest;
	for (int32 Step = 1; Step <= Lines.Num() && Latest.Num() < Count; ++Step)
	{
		const FShuttlegentLogLine& Line = Lines[(Next - Step + Lines.Num()) % Lines.Num()];
		if (Line.Verbosity > MaxVerbosity) continue;
		if (!Category.IsEmpty() && !Line.Category.ToString().Equals(Category, ESearchCase::IgnoreCase)) continue;

		Latest.Add(Line);
	}
	Algo::Reverse(Latest);
	return Latest;
}

void FShuttlegentLogCapture::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
	FShuttlegentLogLine Line;
	Line.Time = FDateTime::Now();
	Line.Category = Category;
	Line.Verbosity = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
	Line.Text = Message;

	FScopeLock ScopeLock(&Lock);
	if (Lines.Num() < ShuttlegentToolsConstant::LogCapacity)
	{
		Lines.Add(MoveTemp(Line));
		Next = Lines.Num() % ShuttlegentToolsConstant::LogCapacity;
		return;
	}

	Lines[Next] = MoveTemp(Line);
	Next = (Next + 1) % ShuttlegentToolsConstant::LogCapacity;
}
