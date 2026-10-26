#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

#include "Tools/FShuttlegentLogLine.h"

/** Keeps the latest editor log lines since the tools module started, for read_log. Lines may arrive on any thread. */
class FShuttlegentLogCapture : public FOutputDevice
{
private:
	mutable FCriticalSection Lock;

	/** Ring buffer of the latest lines; Next is where the next line goes. */
	TArray<FShuttlegentLogLine> Lines;
	int32 Next = 0;

public:
	FShuttlegentLogCapture();

	/** The latest lines, oldest first, at least as severe as MaxVerbosity and in Category when it is set. */
	TArray<FShuttlegentLogLine> GetLatest(int32 Count, ELogVerbosity::Type MaxVerbosity, const FString& Category) const;

	//~ Begin FOutputDevice Interface.
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	//~ End FOutputDevice Interface.
};
