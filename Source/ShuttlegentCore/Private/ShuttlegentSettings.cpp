#include "ShuttlegentSettings.h"

#include "FShuttlegentAgentCatalog.h"

namespace ShuttlegentSettingsPrivate
{
	// 设置面板中的位置：编辑器偏好设置 > Shuttlegent > General
	inline const TCHAR* ContainerName = TEXT("Editor");
	inline const TCHAR* CategoryName = TEXT("Shuttlegent");
	inline const TCHAR* SectionName = TEXT("General");
}

void UShuttlegentSettings::EnsureBuiltins()
{
	// 补回的内置条目沿用现有内置条目的连接；全部被删时用编辑前记下的值
	const FShuttlegentModelEntry* ExistingBuiltin = Models.FindByPredicate([](const FShuttlegentModelEntry& Entry) { return Entry.bBuiltin; });
	const FString FallbackBaseUrl = BuiltinBaseUrlBeforeEdit.IsEmpty() ? FString(ShuttlegentProviderConstant::DeepSeekBaseUrl) : BuiltinBaseUrlBeforeEdit;
	const FString BaseUrl = ExistingBuiltin != nullptr ? ExistingBuiltin->BaseUrl : FallbackBaseUrl;
	const FString ApiKey = ExistingBuiltin != nullptr ? ExistingBuiltin->ApiKey : BuiltinApiKeyBeforeEdit;

	for (int32 BuiltinIndex = 0; BuiltinIndex < UE_ARRAY_COUNT(ShuttlegentBuiltinModel::Ids); ++BuiltinIndex)
	{
		const FString ModelId = ShuttlegentBuiltinModel::Ids[BuiltinIndex];
		int32 EntryIndex = Models.IndexOfByPredicate([&ModelId](const FShuttlegentModelEntry& Entry) { return Entry.bBuiltin && Entry.ModelId == ModelId; });
		if (EntryIndex == INDEX_NONE)
		{
			FShuttlegentModelEntry Added;
			Added.bBuiltin = true;
			Added.BaseUrl = BaseUrl;
			Added.ApiKey = ApiKey;
			EntryIndex = FMath::Min(BuiltinIndex, Models.Num());
			Models.Insert(Added, EntryIndex);
		}

		FShuttlegentModelEntry& Entry = Models[EntryIndex];
		Entry.Name = ShuttlegentBuiltinModel::Names[BuiltinIndex];
		Entry.ModelId = ModelId;
		Entry.Api = EShuttlegentProviderApi::AnthropicMessages;
		Entry.LegacyRoute.Reset();
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

void UShuttlegentSettings::SyncBuiltinConnection(int32 SourceIndex)
{
	if (!Models.IsValidIndex(SourceIndex) || !Models[SourceIndex].bBuiltin) return;

	const FString BaseUrl = Models[SourceIndex].BaseUrl;
	const FString ApiKey = Models[SourceIndex].ApiKey;
	for (FShuttlegentModelEntry& Entry : Models)
	{
		if (!Entry.bBuiltin) continue;

		Entry.BaseUrl = BaseUrl;
		Entry.ApiKey = ApiKey;
	}
}

#if WITH_EDITOR
void UShuttlegentSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	const FShuttlegentModelEntry* Builtin = Models.FindByPredicate([](const FShuttlegentModelEntry& Entry) { return Entry.bBuiltin; });
	if (Builtin == nullptr) return;

	BuiltinBaseUrlBeforeEdit = Builtin->BaseUrl;
	BuiltinApiKeyBeforeEdit = Builtin->ApiKey;
}

void UShuttlegentSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// 改的是某个内置模型的地址或密钥时同步到另一个；再补回被删除的内置条目与默认 Agent
	SyncBuiltinConnection(PropertyChangedEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(UShuttlegentSettings, Models)));
	EnsureBuiltins();

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
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
