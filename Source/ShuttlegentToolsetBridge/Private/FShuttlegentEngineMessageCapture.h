#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

/**
 * Collects what the engine reports on the game thread while one toolset tool runs: warnings, errors and the text of
 * message dialogs, which only log while the tool runs unattended. Listens to GLog for its lifetime.
 */
class FShuttlegentEngineMessageCapture : public FOutputDevice
{
private:
	mutable FCriticalSection Lock;
	TArray<FString> Lines;

public:
	FShuttlegentEngineMessageCapture();
	virtual ~FShuttlegentEngineMessageCapture() override;

	/** The collected lines, oldest first, one per line; empty when the engine reported nothing. */
	FString GetMessages() const;

	//~ Begin FOutputDevice Interface.
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	//~ End FOutputDevice Interface.
};
