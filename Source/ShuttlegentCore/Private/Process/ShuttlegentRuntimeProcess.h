#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"

struct FShuttlegentRuntimeLaunchSpec;
class FRunnable;
class FRunnableThread;
class FEvent;

/**
 * 以子进程运行、标准输入输出重定向的 DeepSeek Harness 运行时。
 *
 * 仅限 Windows。进程以显式的环境块与启动目录挂起启动，加入关闭即终止的 Job Object 后再恢复，
 * 保证它及其子进程不会比编辑器存活更久。
 *
 * 线程：stdin 由专用写线程写入（管道写满也不会阻塞调用方）；stdout 与 stderr 各由一个线程读取并按行切分。
 * 所有公开方法都可在游戏线程安全调用。
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
	 * 启动进程。已有进程在运行时失败。
	 * @param Spec      可执行文件、参数、启动目录与完整环境。
	 * @param OutError  失败原因。
	 */
	bool Launch(const FShuttlegentRuntimeLaunchSpec& Spec, FString& OutError);

	/** 将一行加入 stdin 写入队列，末尾追加 LF。线程安全，不阻塞。 */
	void EnqueueLine(const FString& Line);

	/** 取出一整行 stdout（不含行尾符）。 */
	bool DequeueStdOutLine(FString& OutLine) { return StdOutLines.Dequeue(OutLine); }

	/** 取出一整行 stderr（不含行尾符）。 */
	bool DequeueStdErrLine(FString& OutLine) { return StdErrLines.Dequeue(OutLine); }

	bool IsRunning() const;

	/** 进程退出后的退出码（Shutdown 后仍保留）；运行中或从未启动时为 INDEX_NONE。 */
	int32 GetExitCode() const;

	uint32 GetProcessId() const { return ProcessId; }

	/**
	 * 关闭 stdin 让运行时自行退出，最多等待 TimeoutSeconds，超时则强制结束。
	 * 等待所有 I/O 线程结束并释放全部句柄。可重复调用。
	 */
	void Shutdown(float TimeoutSeconds);

private:
	void ReleaseHandles();
};
