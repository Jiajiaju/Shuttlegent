#pragma once

#include "CoreMinimal.h"

/**
 * 第二个配置补丁，每次启动前写入 DSH_HOME（Shuttlegent.md 第 8.2 节）：用户的 DeepSeek 端点、供应商路由、
 * 默认路由与模型、项目技能目录、用户的 Shell 开关，以及在 PluginDir 下找到的 Shuttlegent 自带运行时插件。
 * 以 JSON 写出，运行时按 YAML 读取。不含任何密钥：供应商只写密钥变量名。
 */
struct FShuttlegentGeneratedPatch
{
	/** 把补丁写入 DshHome；无法写入时返回 false 并给出 OutError。 */
	static bool Write(const FString& DshHome, const FString& PluginDir, bool bShell, FString& OutPath, FString& OutError);
};
