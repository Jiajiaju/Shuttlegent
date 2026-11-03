#pragma once

#include "CoreMinimal.h"

/** 从项目设置与个人设置中读取实际生效的配置（Shuttlegent.md 第 11 节）。仅限游戏线程。 */
struct SHUTTLEGENTCORE_API FShuttlegentProjectConfig
{
	/** 个人设置中某个编辑器工具是否处于开启状态。 */
	static bool IsToolEnabled(const FString& ToolName);

	/** 新会话默认模型所在的路由；没有可用的模型时为内置 DeepSeek 路由。 */
	static FString GetDefaultRoute();

	/** 默认模型所在路由的密钥变量，与默认模型的名称；用于启动检查与缺少密钥的引导卡。 */
	static void GetDefaultProviderKey(FString& OutKeyVariable, FString& OutProviderName);

	/** 运行时可能需要的所有变量：内置 DeepSeek 密钥（网页搜索也会用到）与各路由的密钥。 */
	static TArray<FString> GetKeyVariables();

	/** 项目技能目录中实际存在的那些，其后是插件自带的技能目录（未关闭时），绝对路径；同名技能以先列出的为准。 */
	static TArray<FString> GetSkillDirs();

	/** 项目模板目录中实际存在的那些，其后是插件自带的模板目录（未关闭时），绝对路径；同名模板以先列出的为准。 */
	static TArray<FString> GetTemplateDirs();

	/** 插件自带的模板目录，绝对路径；项目关闭了插件自带技能与模板或目录不存在时为空。 */
	static FString GetBuiltinTemplateDir();
};
