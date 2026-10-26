#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Containers/Ticker.h"

#include "Reference/FShuttlegentReference.h"

class AActor;

DECLARE_MULTICAST_DELEGATE(FOnShuttlegentReferencesChanged);

/**
 * Editor side of draft references: candidates for the @ menu, the selection snapshot, and whether a reference still exists.
 *
 * Assets come from AssetRegistry and are never loaded; the asset list under the search root is gathered on the first search
 * and dropped when assets are added, removed or renamed. Actors come from the editor world. When actors or assets go away
 * (or come back through undo), OnReferencesChanged fires once on the next tick, after the editor finished the change.
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

	/** Level actors and assets whose name contains Query (case-insensitive), names starting with it first, then shorter names. */
	void Search(const FString& Query, TArray<FShuttlegentReference>& OutActors, TArray<FShuttlegentReference>& OutAssets);

	/** Actors selected in the level editor followed by assets selected in the primary Content Browser. */
	TArray<FShuttlegentReference> GetSelection() const;

	/** Whether the actor still lives in the editor world, or the asset is still known to AssetRegistry. */
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
