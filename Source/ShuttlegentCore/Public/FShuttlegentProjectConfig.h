#pragma once

#include "CoreMinimal.h"

struct FShuttlegentProviderEntry;

/** 从项目设置与个人设置中读取实际生效的配置（Shuttlegent.md 第 11 节）。仅限游戏线程。 */
struct SHUTTLEGENTCORE_API FShuttlegentProjectConfig
{
	/** 个人设置中某个编辑器工具是否处于开启状态。 */
	static bool IsToolEnabled(const FString& ToolName);

	/** 新会话使用的路由：用户设置的默认供应商，否则为内置 DeepSeek 路由。 */
	static FString GetDefaultRoute();

	/** 默认路由的密钥变量与显示名；用于启动检查与缺少密钥的引导卡。 */
	static void GetDefaultProviderKey(FString& OutKeyVariable, FString& OutProviderName);

	/** 运行时可能需要的所有变量：内置 DeepSeek 密钥（网页搜索也会用到）与各供应商的密钥。 */
	static TArray<FString> GetKeyVariables();

	/** 把供应商密钥传给运行时所用的变量：SHUTTLEGENT_<ROUTE>_API_KEY。 */
	static FString GetKeyVariable(const FShuttlegentProviderEntry& Entry);

	/** 项目技能目录中实际存在的那些，绝对路径。 */
	static TArray<FString> GetSkillDirs();

	/** 项目模板目录中实际存在的那些，绝对路径。 */
	static TArray<FString> GetTemplateDirs();
};
