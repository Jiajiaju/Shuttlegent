#include "Web/FShuttlegentAssetThumbnails.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/ObjectThumbnail.inl"
#include "Editor/UnrealEdEngine.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "UnrealEdGlobals.h"
#include "UObject/SoftObjectPath.h"

#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentLog.h"

namespace FShuttlegentAssetThumbnailsPrivate
{
	IAssetRegistry* GetAssetRegistry()
	{
		FAssetRegistryModule* Module = FModuleManager::LoadModulePtr<FAssetRegistryModule>(ShuttlegentEditorReference::AssetRegistryModule);
		return Module != nullptr ? &Module->Get() : nullptr;
	}

	/** 按内存缓存、包内保存、现渲染的顺序取缩略图；OutSource 为取到时所用的来源。 */
	bool TryGetThumbnail(const FAssetData& AssetData, FObjectThumbnail& OutThumbnail, const TCHAR*& OutSource)
	{
		const FObjectThumbnail* Cached = ThumbnailTools::FindCachedThumbnail(AssetData.GetFullName());
		if (Cached != nullptr && !Cached->IsEmpty() && Cached->HasValidImageData())
		{
			OutThumbnail = *Cached;
			OutSource = ShuttlegentEditorThumbnail::SourceMemory;
			return true;
		}

		// 只读 .uasset 里保存的缩略图，不加载资产
		if (ThumbnailTools::LoadThumbnailFromPackage(AssetData, OutThumbnail) && !OutThumbnail.IsEmpty() && OutThumbnail.HasValidImageData())
		{
			OutSource = ShuttlegentEditorThumbnail::SourcePackage;
			return true;
		}

		// 最后手段：同步加载资产后渲染
		UObject* Asset = AssetData.GetAsset();
		if (!IsValid(Asset) || GUnrealEd == nullptr) return false;

		// 渲染器画不出的资产（没有可见组件的蓝图、没捕获缩略图的 Niagara 系统）渲染结果是清屏的纯黑，内容浏览器此时改显示类型图标
		UThumbnailManager* ThumbnailManager = GUnrealEd->GetThumbnailManager();
		FThumbnailRenderingInfo* RenderInfo = IsValid(ThumbnailManager) ? ThumbnailManager->GetRenderingInfo(Asset) : nullptr;
		if (RenderInfo == nullptr || !IsValid(RenderInfo->Renderer) || !RenderInfo->Renderer->CanVisualizeAsset(Asset))
		{
			UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent thumbnail: %s cannot be visualized by a thumbnail renderer."), *AssetData.GetObjectPathString());
			return false;
		}

		// 与引擎保存缩略图时一致，不等待纹理流送与着色器编译，避免一次取图卡住编辑器
		OutThumbnail = FObjectThumbnail();
		ThumbnailTools::RenderThumbnail(Asset, ShuttlegentEditorThumbnail::RenderSize, ShuttlegentEditorThumbnail::RenderSize, ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr, &OutThumbnail);
		OutSource = ShuttlegentEditorThumbnail::SourceRender;
		return !OutThumbnail.IsEmpty() && OutThumbnail.HasValidImageData();
	}

	/** 编成不透明的 PNG data URL；渲染得到的缩略图 alpha 可能为 0，直接编码会变成透明图。 */
	FString EncodeDataUrl(const FObjectThumbnail& Thumbnail)
	{
		const FImageView View = Thumbnail.GetImage();
		if (View.RawData == nullptr || View.GetNumPixels() == 0) return FString();

		FImage Image;
		View.CopyTo(Image);
		FImageCore::SetAlphaOpaque(Image);

		TArray64<uint8> Png;
		if (!FImageUtils::CompressImage(Png, ShuttlegentEditorThumbnail::PngExtension, Image) || Png.IsEmpty()) return FString();

		return FString(ShuttlegentEditorThumbnail::DataUrlPrefix) + FBase64::Encode(Png.GetData(), static_cast<uint32>(Png.Num()));
	}
}

void FShuttlegentAssetThumbnails::Initialize(TFunction<void(const FString& Path, const FString& DataUrl)> InDeliver)
{
	Deliver = MoveTemp(InDeliver);
}

void FShuttlegentAssetThumbnails::Shutdown()
{
	if (TickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	Pending.Reset();
	Deliver = nullptr;
}

void FShuttlegentAssetThumbnails::Request(const TArray<FString>& Paths)
{
	int32 Accepted = 0;
	for (const FString& Path : Paths)
	{
		if (Accepted >= ShuttlegentEditorThumbnail::MaxPerRequest) break;
		if (Path.IsEmpty()) continue;

		Pending.AddUnique(Path);
		++Accepted;
	}
	if (Pending.IsEmpty() || TickerHandle.IsValid()) return;

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FShuttlegentAssetThumbnails::Tick));
}

bool FShuttlegentAssetThumbnails::Tick(float DeltaTime)
{
	for (int32 Index = 0; Index < ShuttlegentEditorThumbnail::PerTick && !Pending.IsEmpty(); ++Index)
	{
		const FString Path = Pending[0];
		Pending.RemoveAt(0);

		const FString DataUrl = Fetch(Path);
		if (Deliver) Deliver(Path, DataUrl);
	}
	if (!Pending.IsEmpty()) return true;

	TickerHandle.Reset();
	return false;
}

FString FShuttlegentAssetThumbnails::Fetch(const FString& Path)
{
	IAssetRegistry* AssetRegistry = FShuttlegentAssetThumbnailsPrivate::GetAssetRegistry();
	if (AssetRegistry == nullptr) return FString();

	const FAssetData AssetData = AssetRegistry->GetAssetByObjectPath(FSoftObjectPath(Path));
	if (!AssetData.IsValid())
	{
		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent thumbnail: %s is not a known asset."), *Path);
		return FString();
	}

	FObjectThumbnail Thumbnail;
	const TCHAR* Source = nullptr;
	if (!FShuttlegentAssetThumbnailsPrivate::TryGetThumbnail(AssetData, Thumbnail, Source))
	{
		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent thumbnail: no thumbnail for %s."), *Path);
		return FString();
	}

	const FString DataUrl = FShuttlegentAssetThumbnailsPrivate::EncodeDataUrl(Thumbnail);
	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent thumbnail: %s from %s, %dx%d%s."), *Path, Source, Thumbnail.GetImageWidth(), Thumbnail.GetImageHeight(), DataUrl.IsEmpty() ? TEXT(", encoding failed") : TEXT(""));
	return DataUrl;
}
