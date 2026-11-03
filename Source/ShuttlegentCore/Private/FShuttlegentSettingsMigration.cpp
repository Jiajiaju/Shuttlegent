#include "FShuttlegentSettingsMigration.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "FShuttlegentModelRoutes.h"
#include "FShuttlegentProviderEntry.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentSettings.h"

namespace FShuttlegentSettingsMigrationPrivate
{
	/** 旧版个人设置中与模型有关的值。 */
	struct FLegacySettings
	{
		FString DeepSeekBaseUrl;

		FString DeepSeekApiKey;

		TArray<FShuttlegentProviderEntry> Providers;

		FString DefaultProvider;

		FString DefaultModel;

		/** 旧键至少存在一个。 */
		bool bAnyKey = false;
	};

	/** 默认的“路由 + 模型 ID”，迁移前后用它比较。 */
	struct FDefaultSelection
	{
		FString Route;

		FString ModelId;
	};

	FLegacySettings ReadLegacy(const FString& Section, const FString& File)
	{
		FLegacySettings Legacy;
		Legacy.bAnyKey |= GConfig->GetString(*Section, ShuttlegentMigrationConstant::KeyDeepSeekBaseUrl, Legacy.DeepSeekBaseUrl, File);
		Legacy.bAnyKey |= GConfig->GetString(*Section, ShuttlegentMigrationConstant::KeyDeepSeekApiKey, Legacy.DeepSeekApiKey, File);
		Legacy.bAnyKey |= GConfig->GetString(*Section, ShuttlegentMigrationConstant::KeyDefaultProvider, Legacy.DefaultProvider, File);
		Legacy.bAnyKey |= GConfig->GetString(*Section, ShuttlegentMigrationConstant::KeyDefaultModel, Legacy.DefaultModel, File);

		TArray<FString> ProviderTexts;
		Legacy.bAnyKey |= GConfig->GetArray(*Section, ShuttlegentMigrationConstant::KeyProviders, ProviderTexts, File) > 0;
		for (const FString& Text : ProviderTexts)
		{
			FShuttlegentProviderEntry Entry;
			const TCHAR* Parsed = FShuttlegentProviderEntry::StaticStruct()->ImportText(*Text, &Entry, nullptr, PPF_None, GLog, FShuttlegentProviderEntry::StaticStruct()->GetName());
			if (Parsed == nullptr)
			{
				UE_LOG(LogShuttlegent, Warning, TEXT("Settings migration: an old provider entry could not be read and is skipped."));
				continue;
			}

			Entry.Route.TrimStartAndEndInline();
			Legacy.Providers.Add(MoveTemp(Entry));
		}

		Legacy.DefaultProvider.TrimStartAndEndInline();
		Legacy.DefaultModel.TrimStartAndEndInline();
		return Legacy;
	}

	/** 旧版运行时实际使用的默认：未设默认供应商或供应商不存在时为内置路由的第一个模型。 */
	FDefaultSelection LegacyDefault(const FLegacySettings& Legacy)
	{
		FDefaultSelection Selection { ShuttlegentProviderConstant::BuiltinRoute, ShuttlegentBuiltinModel::Ids[0] };
		if (Legacy.DefaultProvider.IsEmpty()) return Selection;

		if (Legacy.DefaultProvider == ShuttlegentProviderConstant::BuiltinRoute)
		{
			if (!Legacy.DefaultModel.IsEmpty()) Selection.ModelId = Legacy.DefaultModel;
			return Selection;
		}

		const FShuttlegentProviderEntry* Entry = Legacy.Providers.FindByPredicate([&Legacy](const FShuttlegentProviderEntry& Item) { return Item.Route == Legacy.DefaultProvider; });
		if (Entry == nullptr)
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Settings migration: the old default provider %s has no entry; the built-in model is the default."), *Legacy.DefaultProvider);
			return Selection;
		}

		const FShuttlegentProviderModel* First = Entry->Models.FindByPredicate([](const FShuttlegentProviderModel& Model) { return !Model.Id.TrimStartAndEnd().IsEmpty(); });
		Selection.Route = Legacy.DefaultProvider;
		Selection.ModelId = !Legacy.DefaultModel.IsEmpty() ? Legacy.DefaultModel : First != nullptr ? First->Id.TrimStartAndEnd() : FString();
		return Selection;
	}

	/** 迁移后的默认：默认模型所在的路由与模型 ID。 */
	FDefaultSelection CurrentDefault(const UShuttlegentSettings& Settings)
	{
		FDefaultSelection Selection;
		const int32 ModelIndex = FShuttlegentModelRoutes::FindDefaultModel(Settings);
		FShuttlegentModelRoute Route;
		if (!FShuttlegentModelRoutes::FindRoute(FShuttlegentModelRoutes::Build(Settings), ModelIndex, Route)) return Selection;

		Selection.Route = Route.Route;
		Selection.ModelId = Settings.Models[ModelIndex].ModelId.TrimStartAndEnd();
		return Selection;
	}

	/** 与已有模型不重名的名称：重名时加“ (旧路由名)”，仍重名时再加序号。 */
	FString UniqueName(const UShuttlegentSettings& Settings, const FString& Wanted, const FString& Route)
	{
		const auto IsTaken = [&Settings](const FString& Name) { return Settings.Models.ContainsByPredicate([&Name](const FShuttlegentModelEntry& Entry) { return Entry.Name == Name; }); };
		if (!IsTaken(Wanted)) return Wanted;

		const FString WithRoute = Wanted + ShuttlegentMigrationConstant::DuplicateNameOpen + Route + ShuttlegentMigrationConstant::DuplicateNameClose;
		FString Name = WithRoute;
		for (int32 Suffix = 2; IsTaken(Name); ++Suffix)
		{
			Name = WithRoute + ShuttlegentProviderConstant::RouteSeparator + FString::FromInt(Suffix);
		}
		return Name;
	}

	/** 按第 11 节换算：DeepSeek 的地址与密钥给内置模型；每个有路由名的供应商条目的每个模型一条；默认供应商与模型换成默认模型名称。 */
	void Apply(const FLegacySettings& Legacy, const FDefaultSelection& Default, UShuttlegentSettings& Settings)
	{
		const FString BuiltinUrl = Legacy.DeepSeekBaseUrl.TrimStartAndEnd();
		for (FShuttlegentModelEntry& Entry : Settings.Models)
		{
			if (!Entry.bBuiltin) continue;

			Entry.BaseUrl = BuiltinUrl.IsEmpty() ? FString(ShuttlegentProviderConstant::DeepSeekBaseUrl) : BuiltinUrl;
			Entry.ApiKey = Legacy.DeepSeekApiKey.TrimStartAndEnd();
		}

		for (const FShuttlegentProviderEntry& Provider : Legacy.Providers)
		{
			// 旧版运行时忽略没有路由名的条目
			if (Provider.Route.IsEmpty()) continue;

			TArray<FShuttlegentProviderModel> Models = Provider.Models.FilterByPredicate([](const FShuttlegentProviderModel& Model) { return !Model.Id.TrimStartAndEnd().IsEmpty(); });
			const bool bDefaultMissing = Provider.Route == Default.Route && !Default.ModelId.IsEmpty() && !Models.ContainsByPredicate([&Default](const FShuttlegentProviderModel& Model) { return Model.Id.TrimStartAndEnd() == Default.ModelId; });
			if (bDefaultMissing)
			{
				FShuttlegentProviderModel& Added = Models.AddDefaulted_GetRef();
				Added.Id = Default.ModelId;
			}

			for (const FShuttlegentProviderModel& Model : Models)
			{
				const FString ModelId = Model.Id.TrimStartAndEnd();
				const FString Wanted = Model.Name.TrimStartAndEnd().IsEmpty() ? ModelId : Model.Name.TrimStartAndEnd();

				FShuttlegentModelEntry Entry;
				Entry.Name = UniqueName(Settings, Wanted, Provider.Route);
				Entry.ModelId = ModelId;
				Entry.BaseUrl = Provider.BaseUrl.TrimStartAndEnd();
				Entry.Api = Provider.Api;
				Entry.ApiKey = Provider.ApiKey.TrimStartAndEnd();
				Entry.LegacyRoute = Provider.Route;
				Settings.Models.Add(MoveTemp(Entry));
			}
		}

		const bool bBuiltinDefault = Default.Route == ShuttlegentProviderConstant::BuiltinRoute;
		const FShuttlegentModelEntry* DefaultEntry = Settings.Models.FindByPredicate([&Default, bBuiltinDefault](const FShuttlegentModelEntry& Entry)
		{
			return Entry.ModelId == Default.ModelId && (bBuiltinDefault ? Entry.bBuiltin : Entry.LegacyRoute == Default.Route);
		});
		Settings.DefaultModelName = DefaultEntry != nullptr ? DefaultEntry->Name : FString();
	}

	/** 把磁盘上个人设置中的这一节原文备份到 Saved/Shuttlegent/Backup；节不在磁盘上时备份读到的旧值。 */
	bool Backup(const FString& Section, const FString& File, const FLegacySettings& Legacy)
	{
		const FString DiskFile = FConfigCacheIni::GetDestIniFilename(*File, nullptr, *FPaths::GeneratedConfigDir());
		const FString Heading = FString(ShuttlegentMigrationConstant::SectionOpen) + Section + ShuttlegentMigrationConstant::SectionClose;

		TArray<FString> Lines;
		TArray<FString> Kept;
		FFileHelper::LoadFileToStringArray(Lines, *DiskFile);
		bool bInSection = false;
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(ShuttlegentMigrationConstant::SectionOpen)) bInSection = Line.TrimEnd() == Heading;
			if (bInSection) Kept.Add(Line);
		}

		if (Kept.IsEmpty())
		{
			const FString Equals = ShuttlegentMigrationConstant::KeyValueSeparator;
			Kept.Add(Heading);
			Kept.Add(ShuttlegentMigrationConstant::KeyDeepSeekBaseUrl + Equals + Legacy.DeepSeekBaseUrl);
			Kept.Add(ShuttlegentMigrationConstant::KeyDeepSeekApiKey + Equals + Legacy.DeepSeekApiKey);
			Kept.Add(ShuttlegentMigrationConstant::KeyDefaultProvider + Equals + Legacy.DefaultProvider);
			Kept.Add(ShuttlegentMigrationConstant::KeyDefaultModel + Equals + Legacy.DefaultModel);
		}

		const FString Name = FString(ShuttlegentMigrationConstant::BackupPrefix) + FDateTime::Now().ToString(ShuttlegentMigrationConstant::BackupTimeFormat) + ShuttlegentMigrationConstant::BackupExtension;
		const FString Target = FPaths::Combine(FPaths::ProjectSavedDir(), ShuttlegentMigrationConstant::BackupDir, Name);
		if (!FFileHelper::SaveStringArrayToFile(Kept, *Target, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;

		UE_LOG(LogShuttlegent, Display, TEXT("Settings migration: backed up the old settings to %s."), *FPaths::ConvertRelativePathToFull(Target));
		return true;
	}
}

void FShuttlegentSettingsMigration::MigrateIfNeeded()
{
	UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
	if (!IsValid(Settings)) return;

	Settings->EnsureBuiltins();
	if (Settings->bModelsMigrated) return;

	const FString Section = Settings->GetClass()->GetPathName();
	const FString File = Settings->GetClass()->GetConfigName();
	const FShuttlegentSettingsMigrationPrivate::FLegacySettings Legacy = FShuttlegentSettingsMigrationPrivate::ReadLegacy(Section, File);
	if (!Legacy.bAnyKey)
	{
		Settings->bModelsMigrated = true;
		Settings->SaveConfig();
		UE_LOG(LogShuttlegent, Log, TEXT("Settings migration: no old provider settings; nothing to migrate."));
		return;
	}

	if (!FShuttlegentSettingsMigrationPrivate::Backup(Section, File, Legacy))
	{
		UE_LOG(LogShuttlegent, Error, TEXT("Settings migration: could not write the backup; the old settings are kept and migration is retried on the next start."));
		return;
	}

	// 校验不通过时恢复迁移前的内存值，避免本次会话中保存设置时写出半成品
	const TArray<FShuttlegentModelEntry> ModelsBefore = Settings->Models;
	const FString DefaultNameBefore = Settings->DefaultModelName;
	const FShuttlegentSettingsMigrationPrivate::FDefaultSelection Before = FShuttlegentSettingsMigrationPrivate::LegacyDefault(Legacy);
	FShuttlegentSettingsMigrationPrivate::Apply(Legacy, Before, *Settings);

	const FShuttlegentSettingsMigrationPrivate::FDefaultSelection After = FShuttlegentSettingsMigrationPrivate::CurrentDefault(*Settings);
	if (After.Route != Before.Route || After.ModelId != Before.ModelId)
	{
		Settings->Models = ModelsBefore;
		Settings->DefaultModelName = DefaultNameBefore;
		UE_LOG(LogShuttlegent, Error, TEXT("Settings migration: the default model would change from %s/%s to %s/%s; the old settings are kept and migration is retried on the next start."), *Before.Route, *Before.ModelId, *After.Route, *After.ModelId);
		return;
	}

	for (const TCHAR* Key : { ShuttlegentMigrationConstant::KeyDeepSeekBaseUrl, ShuttlegentMigrationConstant::KeyDeepSeekApiKey, ShuttlegentMigrationConstant::KeyProviders, ShuttlegentMigrationConstant::KeyDefaultProvider, ShuttlegentMigrationConstant::KeyDefaultModel })
	{
		GConfig->RemoveKey(*Section, Key, File);
	}

	Settings->bModelsMigrated = true;
	Settings->SaveConfig();
	GConfig->Flush(false, File);
	UE_LOG(LogShuttlegent, Display, TEXT("Settings migration: %d old provider entries migrated to %d models; default model %s/%s unchanged."), Legacy.Providers.Num(), Settings->Models.Num(), *After.Route, *After.ModelId);
}
