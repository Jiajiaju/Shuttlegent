#include "ShuttlegentSettings.h"

#include "FShuttlegentAgentCatalog.h"
#include "FShuttlegentConnectionTester.h"
#include "FShuttlegentModelRoutes.h"

namespace ShuttlegentSettingsPrivate
{
	// 设置面板中的位置：编辑器偏好设置 > Shuttlegent > General
	inline const TCHAR* ContainerName = TEXT("Editor");
	inline const TCHAR* CategoryName = TEXT("Shuttlegent");
	inline const TCHAR* SectionName = TEXT("General");
}

void UShuttlegentSettings::EnsureBuiltins()
{
	// 只留一条内置连接，放在最前；被删时用编辑前记下的密钥加回
	int32 BuiltinIndex = Connections.IndexOfByPredicate([](const FShuttlegentConnection& Connection) { return Connection.bBuiltin; });
	for (int32 Index = Connections.Num() - 1; Index > BuiltinIndex && BuiltinIndex != INDEX_NONE; --Index)
	{
		if (Connections[Index].bBuiltin) Connections.RemoveAt(Index);
	}
	if (BuiltinIndex == INDEX_NONE)
	{
		FShuttlegentConnection Added;
		Added.bBuiltin = true;
		Added.ApiKey = BuiltinApiKeyBeforeEdit;
		Connections.Insert(Added, 0);
	}
	else if (BuiltinIndex != 0)
	{
		const FShuttlegentConnection Moved = Connections[BuiltinIndex];
		Connections.RemoveAt(BuiltinIndex);
		Connections.Insert(Moved, 0);
	}

	FShuttlegentConnection& Builtin = Connections[0];
	Builtin.BaseUrl = ShuttlegentProviderConstant::DeepSeekBaseUrl;
	Builtin.Api = EShuttlegentProviderApi::AnthropicMessages;
	Builtin.ModelIds.Reset();
	for (const TCHAR* ModelId : ShuttlegentBuiltinModel::Ids)
	{
		Builtin.ModelIds.Add(ModelId);
	}

	// Agent 列表与目录对齐：按目录顺序，已有的个人设置保留，新出现的以建议模型为默认模型，目录中已没有的移除
	TArray<FShuttlegentAgentEntry> Synced;
	for (const FShuttlegentAgentManifest& Manifest : FShuttlegentAgentCatalog::GetAgents())
	{
		const FShuttlegentAgentEntry* Existing = Agents.FindByPredicate([&Manifest](const FShuttlegentAgentEntry& Entry) { return Entry.AgentId == Manifest.Id; });
		FShuttlegentAgentEntry& Entry = Synced.Add_GetRef(Existing != nullptr ? *Existing : FShuttlegentAgentEntry {});
		Entry.AgentId = Manifest.Id;
		if (Existing == nullptr) Entry.DefaultModel = Manifest.SuggestedModel;
		if (Manifest.bBuiltin) Entry.bEnabled = true;
	}
	Agents = MoveTemp(Synced);

	const bool bDefaultUsable = Agents.ContainsByPredicate([this](const FShuttlegentAgentEntry& Entry) { return Entry.AgentId == DefaultAgent && Entry.bEnabled; });
	if (!bDefaultUsable) DefaultAgent = ShuttlegentAgentConstant::DefaultAgentId;
}

#if WITH_EDITOR
void UShuttlegentSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	const FShuttlegentConnection* Builtin = Connections.FindByPredicate([](const FShuttlegentConnection& Connection) { return Connection.bBuiltin; });
	if (Builtin != nullptr) BuiltinApiKeyBeforeEdit = Builtin->ApiKey;
}

void UShuttlegentSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	EnsureBuiltins();
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// 地址或密钥改过的连接自动重新检测；检测结果写回本设置并存盘
	for (int32 Index = 0; Index < Connections.Num(); ++Index)
	{
		const FShuttlegentConnection& Connection = Connections[Index];
		if (Connection.Status == EShuttlegentConnectionStatus::Testing || Connection.TestedFingerprint == FShuttlegentModelRoutes::MakeFingerprint(Connection)) continue;

		FShuttlegentConnectionTester::Test(Index);
	}
}
#endif // WITH_EDITOR

FName UShuttlegentSettings::GetContainerName() const
{
	return FName(ShuttlegentSettingsPrivate::ContainerName);
}

FName UShuttlegentSettings::GetCategoryName() const
{
	return FName(ShuttlegentSettingsPrivate::CategoryName);
}

FName UShuttlegentSettings::GetSectionName() const
{
	return FName(ShuttlegentSettingsPrivate::SectionName);
}
