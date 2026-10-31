#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Containers/Ticker.h"

#include "Reference/FShuttlegentReference.h"

class AActor;

DECLARE_MULTICAST_DELEGATE(FOnShuttlegentReferencesChanged);

/**
 * 草稿引用的编辑器侧：@ 菜单的候选项、选中项快照，以及引用是否仍然存在。
 *
 * 资产来自 AssetRegistry，从不加载；搜索根目录下的资产列表在首次搜索时收集，
 * 并在资产新增、移除或重命名时丢弃。Actor 来自编辑器世界。当 Actor 或资产消失
 * （或通过撤销恢复）时，OnReferencesChanged 在编辑器完成该变更后的下一个 tick 触发一次。
 */
class FShuttlegentReferenceFinder : public TSharedFromThis<FShuttlegentReferenceFinder>
{
public:
	FOnShuttlegentReferencesChanged OnReferencesChanged;

private:
	TArray<FAssetData> CachedAssets;
	bool bAssetsCached = false;

	FTSTicker::FDelegateHandle TickerHandle;
	bool bChangePending = false;

	FDelegateHandle ActorAddedHandle;
	FDelegateHandle ActorDeletedHandle;
	FDelegateHandle MapChangeHandle;
	FDelegateHandle UndoRedoHandle;
	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle AssetRenamedHandle;

public:
	void Initialize();
	void Shutdown();

	static FShuttlegentReference FromAsset(const FAssetData& Asset);
	static FShuttlegentReference FromActor(const AActor& Actor);

	/** 名称包含 Query（不区分大小写）的关卡 Actor 与资产；以其开头的名称优先，其次为较短的名称。 */
	void Search(const FString& Query, TArray<FShuttlegentReference>& OutActors, TArray<FShuttlegentReference>& OutAssets);

	/** 关卡编辑器中选中的 Actor，其后为主 Content Browser 中选中的资产。 */
	TArray<FShuttlegentReference> GetSelection() const;

	/** Actor 是否仍存在于编辑器世界中，或资产是否仍为 AssetRegistry 所知。 */
	static bool Exists(const FShuttlegentReference& Reference);

private:
	void GatherAssets();
	void MarkChanged();
	bool HandleTick(float DeltaTime);

	void HandleActorChanged(AActor* Actor);
	void HandleMapChange(uint32 Flags);
	void HandleAssetAdded(const FAssetData& Asset);
	void HandleAssetRemoved(const FAssetData& Asset);
	void HandleAssetRenamed(const FAssetData& Asset, const FString& OldObjectPath);
};
