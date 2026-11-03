#include "FShuttlegentAgentCatalog.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"

namespace FShuttlegentAgentCatalogPrivate
{
	TArray<FShuttlegentAgentManifest>& Cache()
	{
		static TArray<FShuttlegentAgentManifest> Agents;
		return Agents;
	}

	bool& Scanned()
	{
		static bool bValue = false;
		return bValue;
	}

	/** 运行时自带的默认循环：声明全部能力。 */
	FShuttlegentAgentManifest MakeDefault()
	{
		FShuttlegentAgentManifest Manifest;
		Manifest.Id = ShuttlegentAgentConstant::DefaultAgentId;
		Manifest.bBuiltin = true;
		for (const TCHAR* Capability : ShuttlegentAgentConstant::AllCapabilities)
		{
			Manifest.Capabilities.Add(Capability);
		}
		return Manifest;
	}

	/** 读一个 Agent 目录下的说明文件；无效时写警告并返回 false。 */
	bool ReadManifest(const FString& AgentDir, FShuttlegentAgentManifest& OutManifest)
	{
		const FString File = FPaths::Combine(AgentDir, ShuttlegentAgentConstant::ManifestFile);
		FString Text;
		TSharedPtr<FJsonObject> Object;
		if (!FFileHelper::LoadFileToString(Text, *File)) return false;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) || !Object.IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Agent manifest %s is not a JSON object; the agent is skipped."), *File);
			return false;
		}

		FString Entry;
		Object->TryGetStringField(ShuttlegentAgentConstant::FieldId, OutManifest.Id);
		Object->TryGetStringField(ShuttlegentAgentConstant::FieldDisplayName, OutManifest.DisplayName);
		Object->TryGetStringField(ShuttlegentAgentConstant::FieldDescription, OutManifest.Description);
		Object->TryGetStringArrayField(ShuttlegentAgentConstant::FieldCapabilities, OutManifest.Capabilities);
		Object->TryGetStringField(ShuttlegentAgentConstant::FieldSuggestedModel, OutManifest.SuggestedModel);
		Object->TryGetStringField(ShuttlegentAgentConstant::FieldEntry, Entry);

		OutManifest.Id.TrimStartAndEndInline();
		OutManifest.EntryPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(AgentDir, Entry));
		if (OutManifest.Id.IsEmpty() || OutManifest.Id == ShuttlegentAgentConstant::DefaultAgentId || Entry.IsEmpty() || !FPaths::FileExists(OutManifest.EntryPath))
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Agent manifest %s needs an id other than %s and an existing entry module; the agent is skipped."), *File, ShuttlegentAgentConstant::DefaultAgentId);
			return false;
		}
		if (OutManifest.DisplayName.IsEmpty()) OutManifest.DisplayName = OutManifest.Id;
		return true;
	}

	void Scan()
	{
		TArray<FShuttlegentAgentManifest>& Agents = Cache();
		Agents.Reset();
		Agents.Add(MakeDefault());
		Scanned() = true;

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
		if (!Plugin.IsValid()) return;

		const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), ShuttlegentAgentConstant::AgentsDir));
		TArray<FString> Dirs;
		IFileManager::Get().FindFiles(Dirs, *FPaths::Combine(Root, ShuttlegentAgentConstant::DirWildcard), false, true);
		Dirs.Sort();
		for (const FString& Dir : Dirs)
		{
			FShuttlegentAgentManifest Manifest;
			if (!ReadManifest(FPaths::Combine(Root, Dir), Manifest)) continue;
			if (Agents.ContainsByPredicate([&Manifest](const FShuttlegentAgentManifest& Existing) { return Existing.Id == Manifest.Id; }))
			{
				UE_LOG(LogShuttlegent, Warning, TEXT("Agent %s is declared twice; the copy in %s is skipped."), *Manifest.Id, *Dir);
				continue;
			}
			Agents.Add(MoveTemp(Manifest));
		}
		UE_LOG(LogShuttlegent, Log, TEXT("Agent catalog: %d agents (default loop and %d from %s)."), Agents.Num(), Agents.Num() - 1, *Root);
	}
}

const TArray<FShuttlegentAgentManifest>& FShuttlegentAgentCatalog::GetAgents()
{
	if (!FShuttlegentAgentCatalogPrivate::Scanned()) FShuttlegentAgentCatalogPrivate::Scan();
	return FShuttlegentAgentCatalogPrivate::Cache();
}

const FShuttlegentAgentManifest* FShuttlegentAgentCatalog::Find(const FString& AgentId)
{
	return GetAgents().FindByPredicate([&AgentId](const FShuttlegentAgentManifest& Manifest) { return Manifest.Id == AgentId; });
}

void FShuttlegentAgentCatalog::Rescan()
{
	FShuttlegentAgentCatalogPrivate::Scan();
}
