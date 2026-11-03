#include "FShuttlegentGeneratedPatch.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "FShuttlegentAgentCatalog.h"
#include "FShuttlegentModelRoutes.h"
#include "FShuttlegentProjectConfig.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentSettings.h"

namespace FShuttlegentGeneratedPatchPrivate
{
	TSharedRef<FJsonObject> MakeRow(const TCHAR* Id, const TSharedRef<FJsonObject>& Config)
	{
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(ShuttlegentProviderConstant::FieldId, Id);
		Row->SetObjectField(ShuttlegentProviderConstant::FieldConfig, Config);
		return Row;
	}

	/** llm-pi-ai 的一个路由：显示名用路由名（面板只显示模型名），模型取自组内各模型条目。 */
	TSharedRef<FJsonObject> MakeProvider(const FShuttlegentModelRoute& Route, const UShuttlegentSettings& Settings)
	{
		TArray<TSharedPtr<FJsonValue>> Models;
		for (const int32 ModelIndex : Route.ModelIndices)
		{
			const FShuttlegentModelEntry& Model = Settings.Models[ModelIndex];
			const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(ShuttlegentProviderConstant::FieldModelId, Model.ModelId.TrimStartAndEnd());
			Item->SetStringField(ShuttlegentProviderConstant::FieldModelName, Model.Name.TrimStartAndEnd());
			Models.Add(MakeShared<FJsonValueObject>(Item));
		}

		const TSharedRef<FJsonObject> Provider = MakeShared<FJsonObject>();
		Provider->SetStringField(ShuttlegentProviderConstant::FieldDisplayName, Route.Route);
		Provider->SetStringField(ShuttlegentProviderConstant::FieldApiKeyEnv, Route.KeyVariable);
		Provider->SetStringField(ShuttlegentProviderConstant::FieldApi, FShuttlegentModelRoutes::GetApiName(Route.Api));
		if (!Route.BaseUrl.IsEmpty()) Provider->SetStringField(ShuttlegentProviderConstant::FieldBaseUrl, Route.BaseUrl);
		Provider->SetArrayField(ShuttlegentProviderConstant::FieldModels, Models);
		return Provider;
	}

	/** 分发插件的插入行：配置列出已启用的第三方循环（默认循环由分发插件自行加载）。 */
	TSharedRef<FJsonObject> MakeDispatchPlugin(const FString& DispatchPath, const UShuttlegentSettings* Settings)
	{
		TArray<TSharedPtr<FJsonValue>> Agents;
		for (const FShuttlegentAgentManifest& Manifest : FShuttlegentAgentCatalog::GetAgents())
		{
			const FShuttlegentAgentEntry* Entry = IsValid(Settings) ? Settings->Agents.FindByPredicate([&Manifest](const FShuttlegentAgentEntry& Item) { return Item.AgentId == Manifest.Id; }) : nullptr;
			if (Manifest.bBuiltin || (Entry != nullptr && !Entry->bEnabled)) continue;

			const TSharedRef<FJsonObject> Agent = MakeShared<FJsonObject>();
			Agent->SetStringField(ShuttlegentProviderConstant::FieldAgentId, Manifest.Id);
			Agent->SetStringField(ShuttlegentProviderConstant::FieldAgentEntry, Manifest.EntryPath);
			Agents.Add(MakeShared<FJsonValueObject>(Agent));
		}

		const TSharedRef<FJsonObject> Config = MakeShared<FJsonObject>();
		Config->SetArrayField(ShuttlegentProviderConstant::FieldAgents, Agents);

		const TSharedRef<FJsonObject> Plugin = MakeShared<FJsonObject>();
		Plugin->SetStringField(ShuttlegentProviderConstant::FieldId, ShuttlegentProviderConstant::RowAgentDispatch);
		Plugin->SetStringField(ShuttlegentProviderConstant::FieldPluginName, DispatchPath);
		Plugin->SetObjectField(ShuttlegentProviderConstant::FieldConfig, Config);
		return Plugin;
	}
}

bool FShuttlegentGeneratedPatch::Write(const FString& DshHome, const FString& PluginDir, bool bShell, FString& OutPath, FString& OutError)
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	TArray<TSharedPtr<FJsonValue>> Rows;

	// 模型按地址、协议、密钥分组为路由（FShuttlegentModelRoutes）；内置 DeepSeek 路由由 llm-deepseek 提供
	const TArray<FShuttlegentModelRoute> Routes = IsValid(Settings) ? FShuttlegentModelRoutes::Build(*Settings) : TArray<FShuttlegentModelRoute>();
	const FShuttlegentModelRoute* Builtin = Routes.FindByPredicate([](const FShuttlegentModelRoute& Route) { return Route.bBuiltin; });
	const TSharedRef<FJsonObject> DeepSeekConfig = MakeShared<FJsonObject>();
	DeepSeekConfig->SetStringField(ShuttlegentProviderConstant::FieldBaseUrl, Builtin != nullptr ? Builtin->BaseUrl : FString(ShuttlegentProviderConstant::DeepSeekBaseUrl));
	Rows.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeRow(ShuttlegentProviderConstant::RowLlmDeepSeek, DeepSeekConfig)));

	// 其余路由交给 llm-pi-ai，以路由名为键；运行时从各路由的变量读取密钥
	const TSharedRef<FJsonObject> Providers = MakeShared<FJsonObject>();
	for (const FShuttlegentModelRoute& Route : Routes)
	{
		if (!Route.bBuiltin) Providers->SetObjectField(Route.Route, FShuttlegentGeneratedPatchPrivate::MakeProvider(Route, *Settings));
	}
	if (!Providers->Values.IsEmpty())
	{
		const TSharedRef<FJsonObject> Config = MakeShared<FJsonObject>();
		Config->SetObjectField(ShuttlegentProviderConstant::FieldProviders, Providers);
		Rows.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeRow(ShuttlegentProviderConstant::RowLlmPiAi, Config)));
	}

	// 新会话的默认模型
	FShuttlegentModelRoute DefaultRoute;
	const int32 DefaultModel = IsValid(Settings) ? FShuttlegentModelRoutes::FindDefaultModel(*Settings) : INDEX_NONE;
	if (FShuttlegentModelRoutes::FindRoute(Routes, DefaultModel, DefaultRoute))
	{
		const TSharedRef<FJsonObject> Config = MakeShared<FJsonObject>();
		Config->SetStringField(ShuttlegentProviderConstant::FieldProvider, DefaultRoute.Route);
		Config->SetStringField(ShuttlegentProviderConstant::FieldModel, Settings->Models[DefaultModel].ModelId.TrimStartAndEnd());
		Rows.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeRow(ShuttlegentProviderConstant::RowAcp, Config)));
	}

	// 只用项目的技能：运行时的默认技能根目录保持关闭
	TArray<TSharedPtr<FJsonValue>> SkillDirs;
	for (const FString& Dir : FShuttlegentProjectConfig::GetSkillDirs())
	{
		SkillDirs.Add(MakeShared<FJsonValueString>(Dir));
	}
	const TSharedRef<FJsonObject> SkillConfig = MakeShared<FJsonObject>();
	SkillConfig->SetBoolField(ShuttlegentProviderConstant::FieldIncludeDefaultRoots, false);
	SkillConfig->SetArrayField(ShuttlegentProviderConstant::FieldCustomSkillDirs, SkillDirs);
	Rows.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeRow(ShuttlegentProviderConstant::RowSkillFilesystem, SkillConfig)));

	// 基础补丁关闭了 PowerShell 与后台作业工具；用户的 Shell 开关同时重新打开二者（后台作业只服务于 Shell）
	for (const TCHAR* ShellRow : { ShuttlegentProviderConstant::RowToolPwsh, ShuttlegentProviderConstant::RowToolJobs })
	{
		const TSharedRef<FJsonObject> Shell = MakeShared<FJsonObject>();
		Shell->SetStringField(ShuttlegentProviderConstant::FieldId, ShellRow);
		Shell->SetBoolField(ShuttlegentProviderConstant::FieldDisabled, !bShell);
		Rows.Add(MakeShared<FJsonValueObject>(Shell));
	}

	// Shuttlegent 自带的运行时插件：context tap 上报上下文分项，session control 供编辑器分叉会话、执行命令、切换计划模式与审批计划，
	// session tap 推送 ACP 不转发的会话事件；运行时按绝对路径加载插件
	const TPair<const TCHAR*, const TCHAR*> RuntimePlugins[] = {
		{ ShuttlegentProviderConstant::RowContextTap, ShuttlegentPath::ContextTapPlugin },
		{ ShuttlegentProviderConstant::RowSessionControl, ShuttlegentPath::SessionControlPlugin },
		{ ShuttlegentProviderConstant::RowSessionTap, ShuttlegentPath::SessionTapPlugin },
		{ ShuttlegentProviderConstant::RowMemory, ShuttlegentPath::MemoryPlugin },
	};
	TArray<TSharedPtr<FJsonValue>> Plugins;
	for (const TPair<const TCHAR*, const TCHAR*>& RuntimePlugin : RuntimePlugins)
	{
		const FString Path = FPaths::Combine(PluginDir, RuntimePlugin.Value);
		if (!FPaths::FileExists(Path)) continue;

		const TSharedRef<FJsonObject> Plugin = MakeShared<FJsonObject>();
		Plugin->SetStringField(ShuttlegentProviderConstant::FieldId, RuntimePlugin.Key);
		Plugin->SetStringField(ShuttlegentProviderConstant::FieldPluginName, Path);
		Plugins.Add(MakeShared<FJsonValueObject>(Plugin));
	}

	// Agent 分发插件接管工厂槽位，默认循环由它在内部加载，基础补丁的 agent-loop 行停用；插件文件缺失时保持默认循环直接运行
	const FString DispatchPath = FPaths::Combine(PluginDir, ShuttlegentPath::AgentDispatchPlugin);
	if (FPaths::FileExists(DispatchPath))
	{
		Plugins.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeDispatchPlugin(DispatchPath, Settings)));

		const TSharedRef<FJsonObject> AgentLoop = MakeShared<FJsonObject>();
		AgentLoop->SetStringField(ShuttlegentProviderConstant::FieldId, ShuttlegentProviderConstant::RowAgentLoop);
		AgentLoop->SetBoolField(ShuttlegentProviderConstant::FieldDisabled, true);
		Rows.Add(MakeShared<FJsonValueObject>(AgentLoop));
	}
	if (!Plugins.IsEmpty())
	{
		const TSharedRef<FJsonObject> Insert = MakeShared<FJsonObject>();
		Insert->SetArrayField(ShuttlegentProviderConstant::FieldInsert, Plugins);
		Rows.Add(MakeShared<FJsonValueObject>(Insert));
	}

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Rows, Writer);

	OutPath = FPaths::Combine(DshHome, ShuttlegentProviderConstant::GeneratedPatchFile);
	if (FFileHelper::SaveStringToFile(Json, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return true;

	OutError = FString::Printf(TEXT("Cannot write the generated profile patch: %s."), *OutPath);
	return false;
}
