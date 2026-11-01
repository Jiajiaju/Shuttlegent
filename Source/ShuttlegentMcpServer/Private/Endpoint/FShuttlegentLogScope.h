#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/OutputDevice.h"

/** 在对象存活期间收集 Output Log 中的警告与错误，附在写入结果之后；可在任意线程接收日志。 */
class FShuttlegentLogScope : public FOutputDevice
{
private:
	mutable FCriticalSection Lock;
	TArray<FString> Lines;

	/** 超出条数上限而未保留的行数。 */
	int32 Dropped = 0;

public:
	FShuttlegentLogScope();
	virtual ~FShuttlegentLogScope() override;

	//~ Begin FOutputDevice Interface.
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	//~ End FOutputDevice Interface.

	/** 先送达仍在排队的日志，再取出已收集的行；OutDropped 为未保留的行数。 */
	TArray<FString> TakeLines(int32& OutDropped);
};
