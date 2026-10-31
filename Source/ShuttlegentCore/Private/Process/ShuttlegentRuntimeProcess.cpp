#include "Process/ShuttlegentRuntimeProcess.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentRuntimeLaunchSpec.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#endif // PLATFORM_WINDOWS

namespace ShuttlegentRuntimeProcessPrivate
{
	// 写线程空闲时的唤醒间隔，单位毫秒
	inline constexpr uint32 WriterIdleWaitMs = 100;

	// 等待进程正常退出时的轮询间隔，单位秒
	inline constexpr float ExitPollSeconds = 0.05f;

	// ACP stdio 传输的行尾符
	inline constexpr uint8 LineFeed = '\n';
	inline constexpr uint8 CarriageReturn = '\r';

	inline const TCHAR* WriterThreadName = TEXT("ShuttlegentRuntimeStdIn");
	inline const TCHAR* StdOutThreadName = TEXT("ShuttlegentRuntimeStdOut");
	inline const TCHAR* StdErrThreadName = TEXT("ShuttlegentRuntimeStdErr");

#if PLATFORM_WINDOWS
	FString DecodeUtf8(const uint8* Data, int32 Length)
	{
		const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Data), Length);
		return FString(Converter.Length(), Converter.Get());
	}

	/** 读取管道直至 EOF，并把完整的行放入队列。 */
	class FPipeLineReader : public FRunnable
	{
	private:
		HANDLE Pipe = nullptr;
		TQueue<FString, EQueueMode::Mpsc>& Lines;

	public:
		FPipeLineReader(HANDLE InPipe, TQueue<FString, EQueueMode::Mpsc>& InLines)
			: Pipe(InPipe)
			, Lines(InLines)
		{
		}

		//~ Begin FRunnable Interface.
		virtual uint32 Run() override
		{
			TArray<uint8> Pending;
			TArray<uint8> Chunk;
			Chunk.SetNumUninitialized(ShuttlegentProcessConstant::PipeReadChunkBytes);

			while (true)
			{
				DWORD BytesRead = 0;
				const BOOL bRead = ::ReadFile(Pipe, Chunk.GetData(), static_cast<DWORD>(Chunk.Num()), &BytesRead, nullptr);
				if (!bRead || BytesRead == 0) break;

				Pending.Append(Chunk.GetData(), static_cast<int32>(BytesRead));
				EmitCompleteLines(Pending);
			}

			if (Pending.Num() > 0) Lines.Enqueue(DecodeUtf8(Pending.GetData(), Pending.Num()));
			return 0;
		}
		//~ End FRunnable Interface.

	private:
		void EmitCompleteLines(TArray<uint8>& Pending)
		{
			int32 LineStart = 0;
			for (int32 Index = 0; Index < Pending.Num(); ++Index)
			{
				if (Pending[Index] != LineFeed) continue;

				int32 LineEnd = Index;
				if (LineEnd > LineStart && Pending[LineEnd - 1] == CarriageReturn) --LineEnd;

				Lines.Enqueue(DecodeUtf8(Pending.GetData() + LineStart, LineEnd - LineStart));
				LineStart = Index + 1;
			}

			if (LineStart > 0) Pending.RemoveAt(0, LineStart, EAllowShrinking::No);
		}
	};

	/** 把待发送队列写入 stdin；收到关闭请求且队列写完后关闭 stdin。 */
	class FPipeWriter : public FRunnable
	{
	private:
		HANDLE& Pipe;
		TQueue<TArray<uint8>, EQueueMode::Mpsc>& Outbound;
		FEvent* WakeEvent = nullptr;
		FThreadSafeBool bCloseRequested = false;

	public:
		FPipeWriter(HANDLE& InPipe, TQueue<TArray<uint8>, EQueueMode::Mpsc>& InOutbound, FEvent* InWakeEvent)
			: Pipe(InPipe)
			, Outbound(InOutbound)
			, WakeEvent(InWakeEvent)
		{
		}

		//~ Begin FRunnable Interface.
		virtual uint32 Run() override
		{
			while (true)
			{
				WakeEvent->Wait(WriterIdleWaitMs);

				bool bWriteFailed = false;
				TArray<uint8> Bytes;
				while (!bWriteFailed && Outbound.Dequeue(Bytes))
				{
					bWriteFailed = !WriteAll(Bytes);
				}

				if (bWriteFailed || bCloseRequested) break;
			}

			if (Pipe != nullptr)
			{
				::CloseHandle(Pipe);
				Pipe = nullptr;
			}
			return 0;
		}

		virtual void Stop() override
		{
			bCloseRequested = true;
			WakeEvent->Trigger();
		}
		//~ End FRunnable Interface.

	private:
		bool WriteAll(const TArray<uint8>& Bytes)
		{
			int32 Offset = 0;
			while (Offset < Bytes.Num())
			{
				DWORD Written = 0;
				const BOOL bWritten = ::WriteFile(Pipe, Bytes.GetData() + Offset, static_cast<DWORD>(Bytes.Num() - Offset), &Written, nullptr);
				if (!bWritten || Written == 0)
				{
					UE_LOG(LogShuttlegent, Warning, TEXT("Writing to the runtime stdin failed (error %u)."), ::GetLastError());
					return false;
				}

				Offset += static_cast<int32>(Written);
			}
			return true;
		}
	};

	/** 按 CommandLineToArgvW 的规则给单个参数加引号。 */
	FString QuoteArgument(const FString& Argument)
	{
		const bool bNeedsQuotes = Argument.IsEmpty() || Argument.Contains(TEXT(" ")) || Argument.Contains(TEXT("\t")) || Argument.Contains(TEXT("\""));
		if (!bNeedsQuotes) return Argument;

		FString Quoted = TEXT("\"");
		int32 Backslashes = 0;
		for (const TCHAR Character : Argument)
		{
			if (Character == TEXT('\\'))
			{
				++Backslashes;
				continue;
			}

			if (Character == TEXT('"'))
			{
				Quoted.Append(FString::ChrN(Backslashes * 2 + 1, TEXT('\\')));
			}
			else
			{
				Quoted.Append(FString::ChrN(Backslashes, TEXT('\\')));
			}
			Backslashes = 0;
			Quoted.AppendChar(Character);
		}

		Quoted.Append(FString::ChrN(Backslashes * 2, TEXT('\\')));
		Quoted.AppendChar(TEXT('"'));
		return Quoted;
	}

	FString BuildCommandLine(const FShuttlegentRuntimeLaunchSpec& Spec)
	{
		FString CommandLine = QuoteArgument(Spec.Executable);
		for (const FString& Argument : Spec.Arguments)
		{
			CommandLine.AppendChar(TEXT(' '));
			CommandLine.Append(QuoteArgument(Argument));
		}
		return CommandLine;
	}

	/** 构造 CREATE_UNICODE_ENVIRONMENT 环境块：排序后的 NAME=VALUE 条目，各以 NUL 结尾，最后再加一个 NUL。 */
	TArray<TCHAR> BuildEnvironmentBlock(const TMap<FString, FString>& Environment)
	{
		TArray<FString> Names;
		Environment.GetKeys(Names);
		Names.Sort([](const FString& Left, const FString& Right) { return Left.Compare(Right, ESearchCase::IgnoreCase) < 0; });

		TArray<TCHAR> Block;
		for (const FString& Name : Names)
		{
			const FString Entry = Name + TEXT("=") + Environment.FindChecked(Name);
			Block.Append(*Entry, Entry.Len());
			Block.Add(TEXT('\0'));
		}

		Block.Add(TEXT('\0'));
		return Block;
	}

	/** 创建子进程端可继承、父进程端不可继承的管道。 */
	bool CreateChildPipe(HANDLE& OutRead, HANDLE& OutWrite, bool bChildReads)
	{
		SECURITY_ATTRIBUTES Attributes {};
		Attributes.nLength = sizeof(Attributes);
		Attributes.bInheritHandle = TRUE;
		if (!::CreatePipe(&OutRead, &OutWrite, &Attributes, 0)) return false;

		HANDLE ParentEnd = bChildReads ? OutWrite : OutRead;
		return ::SetHandleInformation(ParentEnd, HANDLE_FLAG_INHERIT, 0) != 0;
	}

	void CloseIfValid(HANDLE& Handle)
	{
		if (Handle == nullptr) return;

		::CloseHandle(Handle);
		Handle = nullptr;
	}
#endif // PLATFORM_WINDOWS
}

FShuttlegentRuntimeProcess::FShuttlegentRuntimeProcess()
{
	OutboundEvent = FPlatformProcess::GetSynchEventFromPool(false);
}

FShuttlegentRuntimeProcess::~FShuttlegentRuntimeProcess()
{
	Shutdown(ShuttlegentProcessConstant::GracefulExitTimeoutSeconds);
	FPlatformProcess::ReturnSynchEventToPool(OutboundEvent);
	OutboundEvent = nullptr;
}

bool FShuttlegentRuntimeProcess::Launch(const FShuttlegentRuntimeLaunchSpec& Spec, FString& OutError)
{
#if PLATFORM_WINDOWS
	if (ProcessHandle != nullptr)
	{
		OutError = TEXT("Runtime process is already running.");
		return false;
	}

	HANDLE ChildStdIn = nullptr;
	HANDLE ChildStdOut = nullptr;
	HANDLE ChildStdErr = nullptr;
	HANDLE ParentStdIn = nullptr;
	HANDLE ParentStdOut = nullptr;
	HANDLE ParentStdErr = nullptr;
	const bool bPipes = ShuttlegentRuntimeProcessPrivate::CreateChildPipe(ChildStdIn, ParentStdIn, true)
		&& ShuttlegentRuntimeProcessPrivate::CreateChildPipe(ParentStdOut, ChildStdOut, false)
		&& ShuttlegentRuntimeProcessPrivate::CreateChildPipe(ParentStdErr, ChildStdErr, false);
	if (!bPipes)
	{
		OutError = FString::Printf(TEXT("Creating runtime pipes failed (error %u)."), ::GetLastError());
		ShuttlegentRuntimeProcessPrivate::CloseIfValid(ChildStdIn);
		ShuttlegentRuntimeProcessPrivate::CloseIfValid(ChildStdOut);
		ShuttlegentRuntimeProcessPrivate::CloseIfValid(ChildStdErr);
		ShuttlegentRuntimeProcessPrivate::CloseIfValid(ParentStdIn);
		ShuttlegentRuntimeProcessPrivate::CloseIfValid(ParentStdOut);
		ShuttlegentRuntimeProcessPrivate::CloseIfValid(ParentStdErr);
		return false;
	}

	// 只继承三个子进程管道端，绝不继承编辑器的其他句柄。
	HANDLE InheritedHandles[] = { ChildStdIn, ChildStdOut, ChildStdErr };
	SIZE_T AttributeListBytes = 0;
	::InitializeProcThreadAttributeList(nullptr, 1, 0, &AttributeListBytes);
	TArray<uint8> AttributeListStorage;
	AttributeListStorage.SetNumZeroed(static_cast<int32>(AttributeListBytes));
	LPPROC_THREAD_ATTRIBUTE_LIST AttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(AttributeListStorage.GetData());
	const bool bAttributes = ::InitializeProcThreadAttributeList(AttributeList, 1, 0, &AttributeListBytes)
		&& ::UpdateProcThreadAttribute(AttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, InheritedHandles, sizeof(InheritedHandles), nullptr, nullptr);

	STARTUPINFOEXW StartupInfo {};
	StartupInfo.StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	StartupInfo.StartupInfo.hStdInput = ChildStdIn;
	StartupInfo.StartupInfo.hStdOutput = ChildStdOut;
	StartupInfo.StartupInfo.hStdError = ChildStdErr;
	StartupInfo.lpAttributeList = bAttributes ? AttributeList : nullptr;

	FString CommandLine = ShuttlegentRuntimeProcessPrivate::BuildCommandLine(Spec);
	TArray<TCHAR> EnvironmentBlock = ShuttlegentRuntimeProcessPrivate::BuildEnvironmentBlock(Spec.Environment);
	const DWORD CreationFlags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW | (bAttributes ? EXTENDED_STARTUPINFO_PRESENT : 0);

	PROCESS_INFORMATION ProcessInfo {};
	const BOOL bCreated = ::CreateProcessW(*Spec.Executable, CommandLine.GetCharArray().GetData(), nullptr, nullptr, TRUE, CreationFlags, EnvironmentBlock.GetData(), *Spec.WorkingDirectory, &StartupInfo.StartupInfo, &ProcessInfo);
	const DWORD CreateError = ::GetLastError();
	if (bAttributes) ::DeleteProcThreadAttributeList(AttributeList);

	ShuttlegentRuntimeProcessPrivate::CloseIfValid(ChildStdIn);
	ShuttlegentRuntimeProcessPrivate::CloseIfValid(ChildStdOut);
	ShuttlegentRuntimeProcessPrivate::CloseIfValid(ChildStdErr);

	if (!bCreated)
	{
		OutError = FString::Printf(TEXT("Starting the runtime failed (error %u): %s"), CreateError, *Spec.Executable);
		ShuttlegentRuntimeProcessPrivate::CloseIfValid(ParentStdIn);
		ShuttlegentRuntimeProcessPrivate::CloseIfValid(ParentStdOut);
		ShuttlegentRuntimeProcessPrivate::CloseIfValid(ParentStdErr);
		return false;
	}

	HANDLE Job = ::CreateJobObjectW(nullptr, nullptr);
	if (Job != nullptr)
	{
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION Limits {};
		Limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		::SetInformationJobObject(Job, JobObjectExtendedLimitInformation, &Limits, sizeof(Limits));
		if (!::AssignProcessToJobObject(Job, ProcessInfo.hProcess))
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Assigning the runtime to a Job Object failed (error %u); it may outlive the editor."), ::GetLastError());
		}
	}

	::ResumeThread(ProcessInfo.hThread);
	::CloseHandle(ProcessInfo.hThread);

	ProcessHandle = ProcessInfo.hProcess;
	ProcessId = ProcessInfo.dwProcessId;
	LastExitCode = INDEX_NONE;
	JobHandle = Job;
	StdInWrite = ParentStdIn;
	StdOutRead = ParentStdOut;
	StdErrRead = ParentStdErr;

	Writer = MakeUnique<ShuttlegentRuntimeProcessPrivate::FPipeWriter>(reinterpret_cast<HANDLE&>(StdInWrite), OutboundQueue, OutboundEvent);
	StdOutReader = MakeUnique<ShuttlegentRuntimeProcessPrivate::FPipeLineReader>(StdOutRead, StdOutLines);
	StdErrReader = MakeUnique<ShuttlegentRuntimeProcessPrivate::FPipeLineReader>(StdErrRead, StdErrLines);
	WriterThread = FRunnableThread::Create(Writer.Get(), ShuttlegentRuntimeProcessPrivate::WriterThreadName);
	StdOutThread = FRunnableThread::Create(StdOutReader.Get(), ShuttlegentRuntimeProcessPrivate::StdOutThreadName);
	StdErrThread = FRunnableThread::Create(StdErrReader.Get(), ShuttlegentRuntimeProcessPrivate::StdErrThreadName);

	UE_LOG(LogShuttlegent, Log, TEXT("Runtime started (pid %u): %s"), ProcessId, *CommandLine);
	return true;
#else
	OutError = TEXT("The Shuttlegent runtime is only supported on Windows.");
	return false;
#endif // PLATFORM_WINDOWS
}

void FShuttlegentRuntimeProcess::EnqueueLine(const FString& Line)
{
	const FTCHARToUTF8 Utf8(*Line);
	TArray<uint8> Bytes;
	Bytes.Reserve(Utf8.Length() + 1);
	Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	Bytes.Add(ShuttlegentRuntimeProcessPrivate::LineFeed);

	OutboundQueue.Enqueue(MoveTemp(Bytes));
	if (OutboundEvent != nullptr) OutboundEvent->Trigger();
}

bool FShuttlegentRuntimeProcess::IsRunning() const
{
#if PLATFORM_WINDOWS
	if (ProcessHandle == nullptr) return false;
	return ::WaitForSingleObject(ProcessHandle, 0) == WAIT_TIMEOUT;
#else
	return false;
#endif // PLATFORM_WINDOWS
}

int32 FShuttlegentRuntimeProcess::GetExitCode() const
{
#if PLATFORM_WINDOWS
	if (ProcessHandle == nullptr) return LastExitCode;
	if (IsRunning()) return INDEX_NONE;

	DWORD ExitCode = 0;
	if (!::GetExitCodeProcess(ProcessHandle, &ExitCode)) return INDEX_NONE;
	return static_cast<int32>(ExitCode);
#else
	return INDEX_NONE;
#endif // PLATFORM_WINDOWS
}

void FShuttlegentRuntimeProcess::Shutdown(float TimeoutSeconds)
{
#if PLATFORM_WINDOWS
	if (ProcessHandle == nullptr) return;

	// 关闭 stdin 是 ACP 约定的请求 Agent 退出的方式。
	if (Writer.IsValid()) Writer->Stop();

	const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
	while (IsRunning() && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::Sleep(ShuttlegentRuntimeProcessPrivate::ExitPollSeconds);
	}

	if (IsRunning())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Runtime (pid %u) did not exit within %.1f s; terminating."), ProcessId, TimeoutSeconds);
		if (JobHandle != nullptr)
		{
			::TerminateJobObject(JobHandle, static_cast<UINT>(ShuttlegentProcessConstant::TerminatedExitCode));
		}
		else
		{
			::TerminateProcess(ProcessHandle, static_cast<UINT>(ShuttlegentProcessConstant::TerminatedExitCode));
		}
		::WaitForSingleObject(ProcessHandle, INFINITE);
	}

	// 结束仍占用管道的孙进程，使读线程能读到 EOF。
	if (JobHandle != nullptr) ::TerminateJobObject(JobHandle, static_cast<UINT>(ShuttlegentProcessConstant::TerminatedExitCode));
#endif // PLATFORM_WINDOWS

	ReleaseHandles();
}

void FShuttlegentRuntimeProcess::ReleaseHandles()
{
#if PLATFORM_WINDOWS
	if (WriterThread != nullptr)
	{
		WriterThread->WaitForCompletion();
		delete WriterThread;
		WriterThread = nullptr;
	}
	if (StdOutThread != nullptr)
	{
		StdOutThread->WaitForCompletion();
		delete StdOutThread;
		StdOutThread = nullptr;
	}
	if (StdErrThread != nullptr)
	{
		StdErrThread->WaitForCompletion();
		delete StdErrThread;
		StdErrThread = nullptr;
	}

	Writer.Reset();
	StdOutReader.Reset();
	StdErrReader.Reset();

	if (ProcessHandle != nullptr) LastExitCode = GetExitCode();

	ShuttlegentRuntimeProcessPrivate::CloseIfValid(reinterpret_cast<HANDLE&>(StdInWrite));
	ShuttlegentRuntimeProcessPrivate::CloseIfValid(reinterpret_cast<HANDLE&>(StdOutRead));
	ShuttlegentRuntimeProcessPrivate::CloseIfValid(reinterpret_cast<HANDLE&>(StdErrRead));
	ShuttlegentRuntimeProcessPrivate::CloseIfValid(reinterpret_cast<HANDLE&>(ProcessHandle));
	ShuttlegentRuntimeProcessPrivate::CloseIfValid(reinterpret_cast<HANDLE&>(JobHandle));
	ProcessId = 0;
#endif // PLATFORM_WINDOWS

	TArray<uint8> Discarded;
	while (OutboundQueue.Dequeue(Discarded))
	{
	}
}

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif // PLATFORM_WINDOWS
