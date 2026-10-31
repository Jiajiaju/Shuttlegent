#include "Reference/FShuttlegentReferenceFinder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "Selection.h"
#include "UObject/SoftObjectPath.h"

#include "ShuttlegentEditorConstants.h"

namespace FShuttlegentReferenceFinderPrivate
{
	IAssetRegistry* GetAssetRegistry()
	{
		FAssetRegistryModule* Module = FModuleManager::LoadModulePtr<FAssetRegistryModule>(ShuttlegentEditorReference::AssetRegistryModule);
		return Module != nullptr ? &Module->Get() : nullptr;
	}

	UWorld* GetEditorWorld()
	{
		return GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	/** 以查询词开头的名称排在最前，其次按名称从短到长，最后按字母顺序。 */
	void SortMatches(TArray<FShuttlegentReference>& Matches, const FString& Query)
	{
		Matches.Sort([&Query](const FShuttlegentReference& Left, const FShuttlegentReference& Right)
		{
			const bool bLeftPrefix = Left.Name.StartsWith(Query, ESearchCase::IgnoreCase);
			const bool bRightPrefix = Right.Name.StartsWith(Query, ESearchCase::IgnoreCase);
			if (bLeftPrefix != bRightPrefix) return bLeftPrefix;
			if (Left.Name.Len() != Right.Name.Len()) return Left.Name.Len() < Right.Name.Len();
			return Left.Name < Right.Name;
		});
	}
}

void FShuttlegentReferenceFinder::Initialize()
{
	if (GEngine != nullptr)
	{
		ActorAddedHandle = GEngine->OnLevelActorAdded().AddSP(this, &FShuttlegentReferenceFinder::HandleActorChanged);
		ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddSP(this, &FShuttlegentReferenceFinder::HandleActorChanged);
	}
	MapChangeHandle = FEditorDelegates::MapChange.AddSP(this, &FShuttlegentReferenceFinder::HandleMapChange);
	UndoRedoHandle = FEditorDelegates::PostUndoRedo.AddSP(this, &FShuttlegentReferenceFinder::MarkChanged);

	IAssetRegistry* AssetRegistry = FShuttlegentReferenceFinderPrivate::GetAssetRegistry();
	if (AssetRegistry != nullptr)
	{
		AssetAddedHandle = AssetRegistry->OnAssetAdded().AddSP(this, &FShuttlegentReferenceFinder::HandleAssetAdded);
		AssetRemovedHandle = AssetRegistry->OnAssetRemoved().AddSP(this, &FShuttlegentReferenceFinder::HandleAssetRemoved);
		AssetRenamedHandle = AssetRegistry->OnAssetRenamed().AddSP(this, &FShuttlegentReferenceFinder::HandleAssetRenamed);
	}
}

void FShuttlegentReferenceFinder::Shutdown()
{
	if (TickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();

	if (GEngine != nullptr)
	{
		GEngine->OnLevelActorAdded().Remove(ActorAddedHandle);
		GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
	}
	FEditorDelegates::MapChange.Remove(MapChangeHandle);
	FEditorDelegates::PostUndoRedo.Remove(UndoRedoHandle);

	// 编辑器关闭时注册表可能已不存在；此处绝不加载它
	FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>(ShuttlegentEditorReference::AssetRegistryModule);
	if (Module != nullptr)
	{
		Module->Get().OnAssetAdded().Remove(AssetAddedHandle);
		Module->Get().OnAssetRemoved().Remove(AssetRemovedHandle);
		Module->Get().OnAssetRenamed().Remove(AssetRenamedHandle);
	}
	CachedAssets.Reset();
}

FShuttlegentReference FShuttlegentReferenceFinder::FromAsset(const FAssetData& Asset)
{
	FShuttlegentReference Reference;
	Reference.Kind = EShuttlegentReferenceKind::Asset;
	Reference.Path = Asset.GetObjectPathString();
	Reference.Name = Asset.AssetName.ToString();
	Reference.ClassName = Asset.AssetClassPath.GetAssetName().ToString();
	return Reference;
}

FShuttlegentReference FShuttlegentReferenceFinder::FromActor(const AActor& Actor)
{
	FShuttlegentReference Reference;
	Reference.Kind = EShuttlegentReferenceKind::Actor;
	Reference.Path = Actor.GetPathName();
	Reference.Name = Actor.GetActorLabel();
	Reference.ClassName = Actor.GetClass()->GetName();
	return Reference;
}

void FShuttlegentReferenceFinder::Search(const FString& Query, TArray<FShuttlegentReference>& OutActors, TArray<FShuttlegentReference>& OutAssets)
{
	OutActors.Reset();
	OutAssets.Reset();

	const FString Needle = Query.TrimStartAndEnd();
	UWorld* World = FShuttlegentReferenceFinderPrivate::GetEditorWorld();
	if (IsValid(World))
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor) || !Actor->IsEditable() || !Actor->IsListedInSceneOutliner()) continue;
			if (Needle.IsEmpty() || Actor->GetActorLabel().Contains(Needle, ESearchCase::IgnoreCase)) OutActors.Add(FromActor(*Actor));
		}
	}

	if (!bAssetsCached) GatherAssets();
	for (const FAssetData& Asset : CachedAssets)
	{
		if (Needle.IsEmpty() || Asset.AssetName.ToString().Contains(Needle, ESearchCase::IgnoreCase)) OutAssets.Add(FromAsset(Asset));
	}

	FShuttlegentReferenceFinderPrivate::SortMatches(OutActors, Needle);
	FShuttlegentReferenceFinderPrivate::SortMatches(OutAssets, Needle);
	if (OutActors.Num() > ShuttlegentEditorReference::MaxActorResults) OutActors.SetNum(ShuttlegentEditorReference::MaxActorResults);
	if (OutAssets.Num() > ShuttlegentEditorReference::MaxAssetResults) OutAssets.SetNum(ShuttlegentEditorReference::MaxAssetResults);
}

TArray<FShuttlegentReference> FShuttlegentReferenceFinder::GetSelection() const
{
	TArray<FShuttlegentReference> Selection;

	USelection* SelectedActors = GEditor != nullptr ? GEditor->GetSelectedActors() : nullptr;
	if (SelectedActors != nullptr)
	{
		TArray<AActor*> Actors;
		SelectedActors->GetSelectedObjects<AActor>(Actors);
		for (const AActor* Actor : Actors)
		{
			if (IsValid(Actor)) Selection.Add(FromActor(*Actor));
		}
	}

	FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(ShuttlegentEditorWeb::ContentBrowserModule);
	if (ContentBrowserModule != nullptr)
	{
		TArray<FAssetData> Assets;
		ContentBrowserModule->Get().GetSelectedAssets(Assets);
		for (const FAssetData& Asset : Assets)
		{
			Selection.Add(FromAsset(Asset));
		}
	}
	return Selection;
}

bool FShuttlegentReferenceFinder::Exists(const FShuttlegentReference& Reference)
{
	if (Reference.Kind == EShuttlegentReferenceKind::Actor)
	{
		const AActor* Actor = Cast<AActor>(FSoftObjectPath(Reference.Path).ResolveObject());
		return IsValid(Actor);
	}

	IAssetRegistry* AssetRegistry = FShuttlegentReferenceFinderPrivate::GetAssetRegistry();
	return AssetRegistry != nullptr && AssetRegistry->GetAssetByObjectPath(FSoftObjectPath(Reference.Path)).IsValid();
}

void FShuttlegentReferenceFinder::GatherAssets()
{
	IAssetRegistry* AssetRegistry = FShuttlegentReferenceFinderPrivate::GetAssetRegistry();
	if (AssetRegistry == nullptr) return;

	FARFilter Filter;
	Filter.PackagePaths.Add(ShuttlegentEditorReference::SearchRootPath);
	Filter.bRecursivePaths = true;
	CachedAssets.Reset();
	AssetRegistry->GetAssets(Filter, CachedAssets);
	CachedAssets.RemoveAll([](const FAssetData& Asset) { return Asset.IsRedirector(); });

	// 注册表仍在扫描时，后续搜索继续重新获取
	bAssetsCached = !AssetRegistry->IsLoadingAssets();
}

void FShuttlegentReferenceFinder::MarkChanged()
{
	bChangePending = true;
	if (!TickerHandle.IsValid()) TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FShuttlegentReferenceFinder::HandleTick));
}

bool FShuttlegentReferenceFinder::HandleTick(float DeltaTime)
{
	// 删除事件在 Actor 实际移除之前触发，因此延后一个 tick 再检查引用
	TickerHandle.Reset();
	if (!bChangePending) return false;

	bChangePending = false;
	OnReferencesChanged.Broadcast();
	return false;
}

void FShuttlegentReferenceFinder::HandleActorChanged(AActor* Actor)
{
	MarkChanged();
}

void FShuttlegentReferenceFinder::HandleMapChange(uint32 Flags)
{
	MarkChanged();
}

void FShuttlegentReferenceFinder::HandleAssetAdded(const FAssetData& Asset)
{
	bAssetsCached = false;
}

void FShuttlegentReferenceFinder::HandleAssetRemoved(const FAssetData& Asset)
{
	bAssetsCached = false;
	MarkChanged();
}

void FShuttlegentReferenceFinder::HandleAssetRenamed(const FAssetData& Asset, const FString& OldObjectPath)
{
	bAssetsCached = false;
	MarkChanged();
}
