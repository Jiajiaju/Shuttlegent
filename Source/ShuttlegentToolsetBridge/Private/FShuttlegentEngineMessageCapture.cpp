#include "FShuttlegentEngineMessageCapture.h"

#include "Misc/ScopeLock.h"

#include "ShuttlegentToolsetBridgeConstants.h"

FShuttlegentEngineMessageCapture::FShuttlegentEngineMessageCapture()
{
	if (GLog != nullptr) GLog->AddOutputDevice(this);
}

FShuttlegentEngineMessageCapture::~FShuttlegentEngineMessageCapture()
{
	if (GLog != nullptr) GLog->RemoveOutputDevice(this);
}

FString FShuttlegentEngineMessageCapture::GetMessages() const
{
	FScopeLock ScopeLock(&Lock);
	return FString::Join(Lines, ShuttlegentToolsetBridgeConstant::LineBreak);
}

void FShuttlegentEngineMessageCapture::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
	// Background work (asset builds, streaming) logs from other threads at the same time; only the tool's own thread counts
	if (!IsInGameThread() || Message == nullptr) return;

	const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
	const FString Text(Message);
	const bool bDialog = Text.StartsWith(ShuttlegentToolsetBridgeConstant::MessageDialogPrefix, ESearchCase::IgnoreCase);
	if (Level > ELogVerbosity::Warning && !bDialog) return;

	FScopeLock ScopeLock(&Lock);
	if (Lines.Num() >= ShuttlegentToolsetBridgeConstant::MaxEngineMessages) return;

	Lines.Add(Category.IsNone() ? Text : FString::Printf(TEXT("%s: %s"), *Category.ToString(), *Text));
}
