#include "Settings/FShuttlegentSettingsChoices.h"

#include "Algo/AnyOf.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"

#include "FShuttlegentAgentCatalog.h"
#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentSettings.h"

#define LOCTEXT_NAMESPACE "ShuttlegentSettingsChoices"

namespace FShuttlegentSettingsChoicesPrivate
{
	/** 可选的模型：已连通的连接检测出的模型 ID，按连接顺序，重复的只列一次。 */
	TArray<FString> ModelNames()
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		return IsValid(Settings) ? FShuttlegentModelRoutes::GetModelIds(*Settings) : TArray<FString>();
	}

	/** 已启用的 Agent 标识，按设置中的顺序。 */
	TArray<FString> EnabledAgents()
	{
		TArray<FString> Ids;
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		if (!IsValid(Settings)) return Ids;

		for (const FShuttlegentAgentEntry& Entry : Settings->Agents)
		{
			if (Entry.bEnabled) Ids.Add(Entry.AgentId);
		}
		return Ids;
	}
}

TSharedRef<SWidget> FShuttlegentSettingsChoices::MakeModelCombo(const TSharedRef<IPropertyHandle>& Handle, const FText& EmptyLabel)
{
	const FString Empty = EmptyLabel.ToString();
	return PropertyCustomizationHelpers::MakePropertyComboBox(Handle,
		FOnGetPropertyComboBoxStrings::CreateLambda([Empty](TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, TArray<bool>& OutRestricted)
		{
			if (!Empty.IsEmpty()) OutStrings.Add(MakeShared<FString>(Empty));
			for (const FString& Name : FShuttlegentSettingsChoicesPrivate::ModelNames())
			{
				OutStrings.Add(MakeShared<FString>(Name));
			}
		}),
		FOnGetPropertyComboBoxValue::CreateLambda([Handle, Empty]()
		{
			FString Value;
			Handle->GetValue(Value);
			return Value.IsEmpty() ? Empty : Value;
		}),
		FOnPropertyComboBoxValueSelected::CreateLambda([Handle, Empty](const FString& Selected)
		{
			Handle->SetValue(!Empty.IsEmpty() && Selected == Empty ? FString() : Selected);
		}));
}

TSharedRef<SWidget> FShuttlegentSettingsChoices::MakeAgentCombo(const TSharedRef<IPropertyHandle>& Handle)
{
	return PropertyCustomizationHelpers::MakePropertyComboBox(Handle,
		FOnGetPropertyComboBoxStrings::CreateLambda([](TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, TArray<bool>& OutRestricted)
		{
			for (const FString& AgentId : FShuttlegentSettingsChoicesPrivate::EnabledAgents())
			{
				OutStrings.Add(MakeShared<FString>(GetAgentLabel(AgentId).ToString()));
			}
		}),
		FOnGetPropertyComboBoxValue::CreateLambda([Handle]()
		{
			FString Value;
			Handle->GetValue(Value);
			return GetAgentLabel(Value).ToString();
		}),
		FOnPropertyComboBoxValueSelected::CreateLambda([Handle](const FString& Selected)
		{
			const TArray<FString> Ids = FShuttlegentSettingsChoicesPrivate::EnabledAgents();
			const FString* Found = Ids.FindByPredicate([&Selected](const FString& AgentId) { return GetAgentLabel(AgentId).ToString() == Selected; });
			if (Found != nullptr) Handle->SetValue(*Found);
		}));
}

FText FShuttlegentSettingsChoices::GetAgentLabel(const FString& AgentId)
{
	if (AgentId.IsEmpty() || AgentId == ShuttlegentAgentConstant::DefaultAgentId) return LOCTEXT("DefaultAgent", "默认");

	const FShuttlegentAgentManifest* Manifest = FShuttlegentAgentCatalog::Find(AgentId);
	return FText::FromString(Manifest != nullptr ? Manifest->DisplayName : AgentId);
}

FText FShuttlegentSettingsChoices::GetAgentCapabilities(const FString& AgentId)
{
	// 能力项按 ShuttlegentAgentConstant::AllCapabilities 的顺序显示中文名；说明文件里不认识的能力项原样列出
	const TPair<const TCHAR*, FText> Known[] = {
		{ TEXT("rewind"), LOCTEXT("CapRewind", "回到这里与分支") },
		{ TEXT("slashCommands"), LOCTEXT("CapSlashCommands", "斜杠命令") },
		{ TEXT("planMode"), LOCTEXT("CapPlanMode", "计划模式") },
		{ TEXT("todo"), LOCTEXT("CapTodo", "任务清单") },
		{ TEXT("contextUsage"), LOCTEXT("CapContextUsage", "上下文用量") },
		{ TEXT("memory"), LOCTEXT("CapMemory", "记忆") },
		{ TEXT("modelSwitch"), LOCTEXT("CapModelSwitch", "切换模型") },
		{ TEXT("imageInput"), LOCTEXT("CapImageInput", "图片输入") },
	};

	const FShuttlegentAgentManifest* Manifest = FShuttlegentAgentCatalog::Find(AgentId.IsEmpty() ? FString(ShuttlegentAgentConstant::DefaultAgentId) : AgentId);
	if (Manifest == nullptr || Manifest->Capabilities.IsEmpty()) return LOCTEXT("NoCapabilities", "未声明任何能力");

	TArray<FString> Names;
	for (const TPair<const TCHAR*, FText>& Item : Known)
	{
		if (Manifest->Capabilities.Contains(Item.Key)) Names.Add(Item.Value.ToString());
	}
	for (const FString& Capability : Manifest->Capabilities)
	{
		const bool bKnown = Algo::AnyOf(Known, [&Capability](const TPair<const TCHAR*, FText>& Item) { return Capability == Item.Key; });
		if (!bKnown) Names.Add(Capability);
	}
	return FText::FromString(FString::Join(Names, *LOCTEXT("CapabilitySeparator", "、").ToString()));
}

#undef LOCTEXT_NAMESPACE
