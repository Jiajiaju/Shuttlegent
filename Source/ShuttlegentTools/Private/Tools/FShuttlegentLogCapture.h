#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

#include "Tools/FShuttlegentLogLine.h"

/** 保存自工具模块启动以来最新的编辑器日志行，供 read_log 使用。日志行可能来自任意线程。 */
class FShuttlegentLogCapture : public FOutputDevice
{
private:
	mutable FCriticalSection Lock;

	/** 存放最新日志行的环形缓冲区；Next 为下一行的写入位置。 */
	TArray<FShuttlegentLogLine> Lines;
	int32 Next = 0;

public:
	FShuttlegentLogCapture();

	/** 最新的日志行，按从旧到新排列；严重程度不低于 MaxVerbosity，且设置了 Category 时仅限该分类。 */
	TArray<FShuttlegentLogLine> GetLatest(int32 Count, ELogVerbosity::Type MaxVerbosity, const FString& Category) const;

	//~ Begin FOutputDevice Interface.
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	//~ End FOutputDevice Interface.
};
