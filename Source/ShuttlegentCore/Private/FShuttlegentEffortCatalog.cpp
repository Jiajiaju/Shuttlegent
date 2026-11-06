#include "FShuttlegentEffortCatalog.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"

namespace FShuttlegentEffortCatalogPrivate
{
	FString GetCachePath(const TCHAR* File)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), File);
	}

	/** 比较用的地址：去掉末尾斜杠与版本段、转小写；用户填 https://api.anthropic.com 与目录中的 https://api.anthropic.com/v1 视为同一地址。 */
	FString NormalizeUrl(const FString& Url)
	{
		FString Normalized = FShuttlegentModelRoutes::TrimBaseUrl(Url).ToLower();
		if (Normalized.EndsWith(ShuttlegentProbeConstant::VersionSegment)) Normalized.LeftChopInline(FCString::Strlen(ShuttlegentProbeConstant::VersionSegment));
		return Normalized;
	}

	/** 网关式模型 ID（openai/gpt-5）去掉服务方前缀后的模型名；没有前缀时原样返回。 */
	FString GetName(const FString& ModelId)
	{
		FString Owner;
		FString Name;
		return ModelId.Split(ShuttlegentEffortConstant::IdSeparator, &Owner, &Name, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ? Name : ModelId;
	}

	/** 记录列出的档位中采用的部分，按 Levels 从低到高；没有列出具体档位时为空。 */
	TArray<FString> ReadEfforts(const TSharedPtr<FJsonObject>& Model)
	{
		TArray<FString> Efforts;
		const TSharedPtr<FJsonObject>* Capabilities = nullptr;
		const TSharedPtr<FJsonObject>* Reasoning = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Listed = nullptr;
		if (!Model.IsValid() || !Model->TryGetObjectField(ShuttlegentEffortConstant::FieldCapabilities, Capabilities)) return Efforts;
		if (!(*Capabilities)->TryGetObjectField(ShuttlegentEffortConstant::FieldReasoning, Reasoning)) return Efforts;
		if (!(*Reasoning)->TryGetArrayField(ShuttlegentEffortConstant::FieldEffort, Listed)) return Efforts;

		TSet<FString> Names;
		for (const TSharedPtr<FJsonValue>& Value : *Listed)
		{
			FString Name;
			if (Value.IsValid() && Value->TryGetString(Name)) Names.Add(Name.ToLower());
		}

		for (const TCHAR* Level : ShuttlegentEffortConstant::Levels)
		{
			if (Names.Contains(Level)) Efforts.Add(Level);
		}
		return Efforts;
	}

	/** 记录的某个协议接在 Url 上。 */
	bool ServesUrl(const TSharedPtr<FJsonObject>& Model, const FString& Url)
	{
		const TSharedPtr<FJsonObject>* Api = nullptr;
		if (!Model.IsValid() || !Model->TryGetObjectField(ShuttlegentEffortConstant::FieldApi, Api)) return false;

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Protocol : (*Api)->Values)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			FString BaseUrl;
			if (!Protocol.Value.IsValid() || !Protocol.Value->TryGetObject(Entry) || !(*Entry)->TryGetStringField(ShuttlegentEffortConstant::FieldBaseUrl, BaseUrl)) continue;

			if (NormalizeUrl(BaseUrl) == Url) return true;
		}
		return false;
	}

	/** 目录中 Provider 服务方的 Name 模型记录；没有时返回 nullptr。 */
	const TSharedPtr<FJsonObject>* FindModel(const FJsonObject& Root, const FString& Provider, const FString& Name)
	{
		const TSharedPtr<FJsonObject>* ProviderObject = nullptr;
		const TSharedPtr<FJsonObject>* Models = nullptr;
		const TSharedPtr<FJsonObject>* Model = nullptr;
		if (!Root.TryGetObjectField(Provider, ProviderObject) || !(*ProviderObject)->TryGetObjectField(ShuttlegentEffortConstant::FieldModels, Models)) return nullptr;

		return (*Models)->TryGetObjectField(Name, Model) ? Model : nullptr;
	}

	/**
	 * 一个模型的档位。地址对得上的服务方列出了它时以该记录为准，记录没列档位也不再找别处；
	 * 否则找官方出处：网关式 ID 本身即 canonical_id，或目录中同名记录的 canonical_id。出处有多个且档位说法不一时视为没查到。
	 */
	TArray<FString> FindEfforts(const FJsonObject& Root, const TArray<TSharedPtr<FJsonObject>>& Served, const TMap<FString, TSet<FString>>& Canonicals, const FString& ModelId)
	{
		const TSharedPtr<FJsonObject>* Model = nullptr;
		for (const TSharedPtr<FJsonObject>& Models : Served)
		{
			if (Models->TryGetObjectField(ModelId, Model)) return ReadEfforts(*Model);
		}

		TSet<FString> Candidates;
		if (ModelId.Contains(ShuttlegentEffortConstant::IdSeparator)) Candidates.Add(ModelId);
		for (const FString& Key : { ModelId, GetName(ModelId) })
		{
			const TSet<FString>* Found = Canonicals.Find(Key);
			if (Found != nullptr) Candidates.Append(*Found);
		}

		TArray<FString> Chosen;
		bool bFound = false;
		for (const FString& Canonical : Candidates)
		{
			FString Owner;
			FString Name;
			if (!Canonical.Split(ShuttlegentEffortConstant::IdSeparator, &Owner, &Name)) continue;

			const TSharedPtr<FJsonObject>* Official = FindModel(Root, Owner, Name);
			if (Official == nullptr) continue;

			const TArray<FString> Efforts = ReadEfforts(*Official);
			if (bFound && Efforts != Chosen) return TArray<FString>();

			Chosen = Efforts;
			bFound = true;
		}
		return Chosen;
	}

	/** 解析目录正文并为 ModelIds 逐个查档位；正文不是目录时 bOutParsed 为 false。在工作线程运行。 */
	TArray<FShuttlegentModelEfforts> Match(const FString& Body, const FString& Url, const TArray<FString>& ModelIds, bool& bOutParsed)
	{
		TArray<FShuttlegentModelEfforts> Result;
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		bOutParsed = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
		if (!bOutParsed) return Result;

		TSet<FString> Wanted;
		for (const FString& ModelId : ModelIds)
		{
			Wanted.Add(ModelId);
			Wanted.Add(GetName(ModelId));
		}

		// 一次遍历：收集地址对得上的服务方，以及要找的模型名在各处记录的 canonical_id
		TArray<TSharedPtr<FJsonObject>> Served;
		TMap<FString, TSet<FString>> Canonicals;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Provider : Root->Values)
		{
			const TSharedPtr<FJsonObject>* ProviderObject = nullptr;
			const TSharedPtr<FJsonObject>* Models = nullptr;
			if (!Provider.Value.IsValid() || !Provider.Value->TryGetObject(ProviderObject) || !(*ProviderObject)->TryGetObjectField(ShuttlegentEffortConstant::FieldModels, Models)) continue;

			bool bServes = false;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Model : (*Models)->Values)
			{
				const TSharedPtr<FJsonObject>* ModelObject = nullptr;
				if (!Model.Value.IsValid() || !Model.Value->TryGetObject(ModelObject)) continue;

				if (!bServes) bServes = ServesUrl(*ModelObject, Url);

				FString Canonical;
				if (Wanted.Contains(Model.Key) && (*ModelObject)->TryGetStringField(ShuttlegentEffortConstant::FieldCanonicalId, Canonical) && !Canonical.IsEmpty()) Canonicals.FindOrAdd(Model.Key).Add(Canonical);
			}
			if (bServes) Served.Add(*Models);
		}

		for (const FString& ModelId : ModelIds)
		{
			TArray<FString> Efforts = FindEfforts(*Root, Served, Canonicals, ModelId);
			if (Efforts.IsEmpty()) continue;

			FShuttlegentModelEfforts& Item = Result.AddDefaulted_GetRef();
			Item.ModelId = ModelId;
			Item.Efforts = MoveTemp(Efforts);
		}
		return Result;
	}

	/** 在工作线程解析目录，回到游戏线程交出结果。 */
	void Parse(FString Body, const FString& Url, const TArray<FString>& ModelIds, FShuttlegentEffortCatalog::FOnResolved OnDone)
	{
		Async(EAsyncExecution::ThreadPool, [Body = MoveTemp(Body), Url, ModelIds, OnDone]()
		{
			bool bParsed = false;
			TArray<FShuttlegentModelEfforts> Efforts = Match(Body, Url, ModelIds, bParsed);
			const int32 ModelCount = ModelIds.Num();
			AsyncTask(ENamedThreads::GameThread, [OnDone, bParsed, Efforts = MoveTemp(Efforts), ModelCount]()
			{
				if (bParsed)
				{
					UE_LOG(LogShuttlegent, Log, TEXT("Reasoning efforts: %d of %d model(s) found in the model catalog."), Efforts.Num(), ModelCount);
				}
				else
				{
					UE_LOG(LogShuttlegent, Warning, TEXT("Reasoning efforts: the model catalog could not be parsed."));
				}
				OnDone.ExecuteIfBound(bParsed, Efforts);
			});
		});
	}

	/** 下载不到新目录时用本地缓存；也没有缓存时判为目录取不到。 */
	void ParseCache(const FString& Url, const TArray<FString>& ModelIds, FShuttlegentEffortCatalog::FOnResolved OnDone)
	{
		FString Body;
		if (FFileHelper::LoadFileToString(Body, *GetCachePath(ShuttlegentEffortConstant::CacheFile)))
		{
			Parse(MoveTemp(Body), Url, ModelIds, OnDone);
			return;
		}

		UE_LOG(LogShuttlegent, Warning, TEXT("Reasoning efforts: no model catalog is available (no download and no cached copy); models keep the provider's default."));
		OnDone.ExecuteIfBound(false, TArray<FShuttlegentModelEfforts>());
	}
}

void FShuttlegentEffortCatalog::Resolve(const FString& BaseUrl, const TArray<FString>& ModelIds, FOnResolved OnDone)
{
	const FString Url = FShuttlegentEffortCatalogPrivate::NormalizeUrl(BaseUrl);
	const FString CachePath = FShuttlegentEffortCatalogPrivate::GetCachePath(ShuttlegentEffortConstant::CacheFile);
	const FString EtagPath = FShuttlegentEffortCatalogPrivate::GetCachePath(ShuttlegentEffortConstant::EtagFile);
	FString Etag;
	const bool bCached = FPaths::FileExists(CachePath) && FFileHelper::LoadFileToString(Etag, *EtagPath);

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ShuttlegentEffortConstant::CatalogUrl);
	Request->SetVerb(ShuttlegentProbeConstant::VerbGet);
	Request->SetTimeout(ShuttlegentEffortConstant::TimeoutSeconds);
	if (bCached && !Etag.IsEmpty()) Request->SetHeader(ShuttlegentEffortConstant::HeaderIfNoneMatch, Etag);

	Request->OnProcessRequestComplete().BindLambda([Url, ModelIds, OnDone, CachePath, EtagPath](FHttpRequestPtr Completed, FHttpResponsePtr Response, bool bConnected)
	{
		const int32 StatusCode = bConnected && Response.IsValid() ? Response->GetResponseCode() : 0;
		if (StatusCode >= ShuttlegentProbeConstant::StatusOkMin && StatusCode <= ShuttlegentProbeConstant::StatusOkMax)
		{
			FString Body = Response->GetContentAsString();
			FFileHelper::SaveStringToFile(Body, *CachePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			FFileHelper::SaveStringToFile(Response->GetHeader(ShuttlegentEffortConstant::HeaderEtag), *EtagPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			UE_LOG(LogShuttlegent, Log, TEXT("Reasoning efforts: downloaded the model catalog (%d characters)."), Body.Len());
			FShuttlegentEffortCatalogPrivate::Parse(MoveTemp(Body), Url, ModelIds, OnDone);
			return;
		}

		if (StatusCode == ShuttlegentEffortConstant::StatusNotModified)
		{
			UE_LOG(LogShuttlegent, Log, TEXT("Reasoning efforts: the model catalog is unchanged; using the cached copy."));
		}
		else if (StatusCode == 0)
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Reasoning efforts: could not download the model catalog (no response); using the cached copy if any."));
		}
		else
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Reasoning efforts: could not download the model catalog (HTTP %d); using the cached copy if any."), StatusCode);
		}
		FShuttlegentEffortCatalogPrivate::ParseCache(Url, ModelIds, OnDone);
	});
	Request->ProcessRequest();
}
