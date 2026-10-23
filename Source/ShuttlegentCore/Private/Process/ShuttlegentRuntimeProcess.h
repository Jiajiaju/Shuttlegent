#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"

struct FShuttlegentRuntimeLaunchSpec;
class FRunnable;
class FRunnableThread;
class FEvent;

/**
 * The DeepSeek Harness runtime as a child process with redirected stdio.
 *
 * Windows only. The process starts suspended with an explicit environment block and
 * launch directory, is assigned to a kill-on-close Job Object, then resumed, so it and
 * its children never outlive the editor.
 *
 * Threading: stdin is written by a dedicated writer thread (a full pipe never blocks
 * the caller); stdout and stderr are read by one thread each and split into lines.
 * Every public method is safe to call from the game thread.
 */
class FShuttlegentRuntimeProcess
{
private:
	void* ProcessHandle = nullptr;
	void* JobHandle = nullptr;
	void* StdInWrite = nullptr;
	void* StdOutRead = nullptr;
	void* StdErrRead = nullptr;
	uint32 ProcessId = 0;
	int32 LastExitCode = INDEX_NONE;

	TQueue<TArray<uint8>, EQueueMode::Mpsc> OutboundQueue;
	TQueue<FString, EQueueMode::Mpsc> StdOutLines;
	TQueue<FString, EQueueMode::Mpsc> StdErrLines;

	FEvent* OutboundEvent = nullptr;
	TUniquePtr<FRunnable> Writer;
	TUniquePtr<FRunnable> StdOutReader;
	TUniquePtr<FRunnable> StdErrReader;
	FRunnableThread* WriterThread = nullptr;
	FRunnableThread* StdOutThread = nullptr;
	FRunnableThread* StdErrThread = nullptr;

public:
	FShuttlegentRuntimeProcess();
	~FShuttlegentRuntimeProcess();

	FShuttlegentRuntimeProcess(const FShuttlegentRuntimeProcess&) = delete;
	FShuttlegentRuntimeProcess& operator=(const FShuttlegentRuntimeProcess&) = delete;

	/**
	 * Starts the process. Fails when a process is already running.
	 * @param Spec      Executable, arguments, launch directory and the complete environment.
	 * @param OutError  Reason on failure.
	 */
	bool Launch(const FShuttlegentRuntimeLaunchSpec& Spec, FString& OutError);

	/** Queues one line for stdin; a trailing LF is appended. Thread-safe and non-blocking. */
	void EnqueueLine(const FString& Line);

	/** Pops one complete stdout line (without the line terminator). */
	bool DequeueStdOutLine(FString& OutLine) { return StdOutLines.Dequeue(OutLine); }

	/** Pops one complete stderr line (without the line terminator). */
	bool DequeueStdErrLine(FString& OutLine) { return StdErrLines.Dequeue(OutLine); }

	bool IsRunning() const;

	/** Exit code once the process has exited (kept after Shutdown); INDEX_NONE while running or never started. */
	int32 GetExitCode() const;

	uint32 GetProcessId() const { return ProcessId; }

	/**
	 * Closes stdin so the runtime exits on its own, waits up to TimeoutSeconds, then terminates
	 * it. Joins all I/O threads and releases every handle. Safe to call repeatedly.
	 */
	void Shutdown(float TimeoutSeconds);

private:
	void ReleaseHandles();
};
