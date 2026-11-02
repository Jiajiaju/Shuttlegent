#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

/**
 * 回答中 ![说明](ue-asset:...) 的资产缩略图：按需抓取，不预先导出。
 *
 * 页面把一条回答里的资产路径一次请求过来；路径进入队列，每个 Core Ticker tick 只取一张并立即交给页面，
 * 一次几十张也不会叠成一次长卡顿。依次尝试内存缓存、包内保存的缩略图（不加载资产）、加载资产后现渲染。
 */
class FShuttlegentAssetThumbnails : public TSharedFromThis<FShuttlegentAssetThumbnails>
{
private:
	/** 取到一张（或确认取不到，DataUrl 为空）时调用；Path 与请求时一致。 */
	TFunction<void(const FString& Path, const FString& DataUrl)> Deliver;

	TArray<FString> Pending;
	FTSTicker::FDelegateHandle TickerHandle;

public:
	void Initialize(TFunction<void(const FString& Path, const FString& DataUrl)> InDeliver);
	void Shutdown();

	/** 资产对象路径加入队列；已在队列中的不重复加入，超过单次上限的忽略。 */
	void Request(const TArray<FString>& Paths);

private:
	bool Tick(float DeltaTime);

	/** 资产缩略图的 PNG data URL；资产不存在或没有缩略图时为空。 */
	static FString Fetch(const FString& Path);
};
