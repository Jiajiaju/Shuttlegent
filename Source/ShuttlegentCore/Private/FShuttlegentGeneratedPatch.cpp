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

	/**
	 * llm-pi-ai 的一个路由：显示名用路由名（面板只显示模型名），模型取自连接检测出的模型，名称即模型 ID。
	 * 查得档位的模型声明 reasoningEfforts，每档按档位名原样发送；不声明关闭档，未选档位的请求因此不带推理参数，沿用服务方的默认行为。
	 */
	TSharedRef<FJsonObject> MakeProvider(const FShuttlegentModelRoute& Route)
	{
		TArray<TSharedPtr<FJsonValue>> Models;
		for (const FString& ModelId : Route.ModelIds)
		{
			const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(ShuttlegentProviderConstant::FieldModelId, ModelId);
			Item->SetStringField(ShuttlegentProviderConstant::FieldModelName, ModelId);

			const TArray<FString>* Efforts = Route.ModelEfforts.Find(ModelId);
			if (Efforts != nullptr && !Efforts->IsEmpty())
			{
				const TSharedRef<FJsonObject> Declared = MakeShared<FJsonObject>();
				for (const FString& Effort : *Efforts)
				{
					Declared->SetStringField(Effort, Effort);
				}
				Item->SetObjectField(ShuttlegentProviderConstant::FieldReasoningEfforts, Declared);
			}
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

	// 每条可用的模型连接一个路由（FShuttlegentModelRoutes）；内置 DeepSeek 路由由 llm-deepseek 提供
	const TArray<FShuttlegentModelRoute> Routes = IsValid(Settings) ? FShuttlegentModelRoutes::Build(*Settings) : TArray<FShuttlegentModelRoute>();
	const FShuttlegentModelRoute* Builtin = Routes.FindByPredicate([](const FShuttlegentModelRoute& Route) { return Route.bBuiltin; });
	const TSharedRef<FJsonObject> DeepSeekConfig = MakeShared<FJsonObject>();
	DeepSeekConfig->SetStringField(ShuttlegentProviderConstant::FieldBaseUrl, Builtin != nullptr ? Builtin->BaseUrl : FString(ShuttlegentProviderConstant::DeepSeekBaseUrl));
	Rows.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeRow(ShuttlegentProviderConstant::RowLlmDeepSeek, DeepSeekConfig)));

	// 其余路由交给 llm-pi-ai，以路由名为键；运行时从各路由的变量读取密钥
	const TSharedRef<FJsonObject> Providers = MakeShared<FJsonObject>();
	for (const FShuttlegentModelRoute& Route : Routes)
	{
		if (!Route.bBuiltin) Providers->SetObjectField(Route.Route, FShuttlegentGeneratedPatchPrivate::MakeProvider(Route));
	}
	if (!Providers->Values.IsEmpty())
	{
		const TSharedRef<FJsonObject> Config = MakeShared<FJsonObject>();
		Config->SetObjectField(ShuttlegentProviderConstant::FieldProviders, Providers);
		Rows.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeRow(ShuttlegentProviderConstant::RowLlmPiAi, Config)));
	}

	// 新会话的默认模型：第一条已连通的连接中的第一个模型
	FShuttlegentModelRoute DefaultRoute;
	FString DefaultModelId;
	if (IsValid(Settings) && FShuttlegentModelRoutes::FindDefaultModel(*Settings, DefaultRoute, DefaultModelId))
	{
		const TSharedRef<FJsonObject> Config = MakeShared<FJsonObject>();
		Config->SetStringField(ShuttlegentProviderConstant::FieldProvider, DefaultRoute.Route);
		Config->SetStringField(ShuttlegentProviderConstant::FieldModel, DefaultModelId);
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
