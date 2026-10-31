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
	// 后台任务（资源构建、流送）会同时从其他线程写日志；只统计工具自身所在线程的消息
	if (!IsInGameThread() || Message == nullptr) return;

	const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
	const FString Text(Message);
	const bool bDialog = Text.StartsWith(ShuttlegentToolsetBridgeConstant::MessageDialogPrefix, ESearchCase::IgnoreCase);
	if (Level > ELogVerbosity::Warning && !bDialog) return;

	FScopeLock ScopeLock(&Lock);
	if (Lines.Num() >= ShuttlegentToolsetBridgeConstant::MaxEngineMessages) return;

	Lines.Add(Category.IsNone() ? Text : FString::Printf(TEXT("%s: %s"), *Category.ToString(), *Text));
}
