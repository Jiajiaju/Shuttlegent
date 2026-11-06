#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentModelEfforts.h"

/**
 * 从 models.opencode.ai 的模型目录查模型的推理强度档位。目录缓存在项目 Saved 目录，按 ETag 只在有变化时重新下载；
 * 下载失败时用缓存。匹配分两步：先按连接地址找到目录中的服务方，在其中找模型；地址对不上时按模型的官方出处（canonical_id）找。
 * 只采用列出具体档位的记录，其余模型视为没查到。目录在工作线程解析。仅限游戏线程调用，回调也在游戏线程。
 */
struct FShuttlegentEffortCatalog
{
	/** bAvailable 为 false 表示目录取不到（没有网络也没有缓存），此时 Efforts 为空，不代表这些模型没有档位。 */
	DECLARE_DELEGATE_TwoParams(FOnResolved, bool /*bAvailable*/, const TArray<FShuttlegentModelEfforts>& /*Efforts*/);

	/** 查 BaseUrl 这条连接下 ModelIds 各模型的档位；OnDone 恰好调用一次。 */
	static void Resolve(const FString& BaseUrl, const TArray<FString>& ModelIds, FOnResolved OnDone);
};
