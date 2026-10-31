#pragma once

#include "CoreMinimal.h"

#include "ShuttlegentStartIssue.h"

/** 启动 DeepSeek Harness 运行时进程所需的全部信息。 */
struct FShuttlegentRuntimeLaunchSpec
{
	/** 运行时可执行文件的绝对路径。 */
	FString Executable;

	/** 命令行参数，未加引号。 */
	TArray<FString> Arguments;

	/** 进程的启动目录（DSH_HOME，位于系统盘）。 */
	FString WorkingDirectory;

	/** 进程的完整环境；不继承其他任何变量。 */
	TMap<FString, FString> Environment;

	/**
	 * 根据插件目录结构与个人设置（含供应商密钥）构造启动参数。
	 * @param OutSpec   成功时填入。
	 * @param OutError  失败时的可读原因；只写路径与变量名，绝不写密钥值。
	 * @param OutIssue  供启动引导卡使用的失败类型；成功时为 None。
	 * @return 运行时可执行文件与配置补丁存在、DSH_HOME 可创建且已设置供应商密钥时返回 true。
	 */
	static bool Build(FShuttlegentRuntimeLaunchSpec& OutSpec, FString& OutError, EShuttlegentStartIssue& OutIssue);
};
