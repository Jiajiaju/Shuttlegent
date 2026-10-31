#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

/**
 * 收集单个 Toolset 工具运行期间引擎在游戏线程上报告的内容：警告、错误，以及消息对话框的文本
 * （工具以无人值守方式运行时，消息对话框只写入日志）。在其生命周期内监听 GLog。
 */
class FShuttlegentEngineMessageCapture : public FOutputDevice
{
private:
	mutable FCriticalSection Lock;
	TArray<FString> Lines;

public:
	FShuttlegentEngineMessageCapture();
	virtual ~FShuttlegentEngineMessageCapture() override;

	/** 已收集的消息，按时间由旧到新，每条一行；引擎未报告任何内容时为空。 */
	FString GetMessages() const;

	//~ Begin FOutputDevice Interface.
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	//~ End FOutputDevice Interface.
};
