#pragma once

#include "CoreMinimal.h"

/**
 * 把旧版供应商设置（DeepSeekBaseUrl、DeepSeekApiKey、Providers、DefaultProvider、DefaultModel）一次性迁移到模型、Agent、默认值。
 * 迁移前把个人设置中 Shuttlegent 一节备份到 Saved/Shuttlegent/Backup；
 * 迁移前后默认的“路由 + 模型 ID”必须一致，一致才删除旧键并保存，否则保留旧键、下次启动重试。结果写入 Output Log。
 */
struct FShuttlegentSettingsMigration
{
	/** 模块启动时调用：补齐内置模型与默认 Agent；尚未迁移时执行迁移。 */
	static void MigrateIfNeeded();
};
