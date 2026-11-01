#include "Endpoint/FShuttlegentLogScope.h"

#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"

#include "ShuttlegentMcpServerConstants.h"

FShuttlegentLogScope::FShuttlegentLogScope()
{
	if (GLog != nullptr) GLog->AddOutputDevice(this);
}

FShuttlegentLogScope::~FShuttlegentLogScope()
{
	if (GLog == nullptr) return;

	// 日志线程可能仍持有本对象，送达后再移除
	GLog->FlushThreadedLogs();
	GLog->RemoveOutputDevice(this);
}

void FShuttlegentLogScope::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
	const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
	if (Message == nullptr || Level == ELogVerbosity::NoLogging || Level > ELogVerbosity::Warning) return;

	const FString Line = FString::Printf(TEXT("%s: %s: %s"), *Category.ToString(), ToString(Level), Message).Left(ShuttlegentMcpTool::MaxWriteLogLineChars);

	FScopeLock ScopeLock(&Lock);
	if (!Lines.IsEmpty() && Lines.Last() == Line) return;
	if (Lines.Num() >= ShuttlegentMcpTool::MaxWriteLogLines)
	{
		++Dropped;
		return;
	}
	Lines.Add(Line);
}

TArray<FString> FShuttlegentLogScope::TakeLines(int32& OutDropped)
{
	if (GLog != nullptr) GLog->FlushThreadedLogs();

	FScopeLock ScopeLock(&Lock);
	OutDropped = Dropped;
	Dropped = 0;
	return MoveTemp(Lines);
}
