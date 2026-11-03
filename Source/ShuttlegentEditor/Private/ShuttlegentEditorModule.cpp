#include "ShuttlegentEditorModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#include "Chat/ShuttlegentChatModel.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentCoreModule.h"
#include "FShuttlegentAgentEntry.h"
#include "FShuttlegentModelEntry.h"
#include "Settings/FShuttlegentAgentEntryCustomization.h"
#include "Settings/FShuttlegentModelEntryCustomization.h"
#include "Settings/FShuttlegentSettingsCustomization.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentSettings.h"
#include "ShuttlegentStyle.h"
#include "Web/FShuttlegentPanelCommands.h"
#include "Web/SShuttlegentWebPanel.h"

#define LOCTEXT_NAMESPACE "ShuttlegentEditor"

void FShuttlegentEditorModule::StartupModule()
{
	FShuttlegentStyle::Register();
	FShuttlegentPanelCommands::Register();

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(ShuttlegentEditorSettingsUi::PropertyEditorModule);
	PropertyEditor.RegisterCustomClassLayout(UShuttlegentSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FShuttlegentSettingsCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(FShuttlegentModelEntry::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FShuttlegentModelEntryCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(FShuttlegentAgentEntry::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FShuttlegentAgentEntryCustomization::MakeInstance));

	ChatModel = MakeShared<FShuttlegentChatModel>();
	const TSharedPtr<FShuttlegentAgentService> Service = FShuttlegentCoreModule::Get().GetAgentService();
	if (Service.IsValid()) ChatModel->Initialize(Service.ToSharedRef());

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ShuttlegentEditorName::ChatTabId, FOnSpawnTab::CreateRaw(this, &FShuttlegentEditorModule::SpawnChatTab))
		.SetDisplayName(LOCTEXT("ChatTabTitle", "Shuttlegent"))
		.SetTooltipText(LOCTEXT("ChatTabTooltip", "在编辑器中与 AI Agent 协作"))
		.SetIcon(FSlateIcon(ShuttlegentEditorName::StyleSetName, ShuttlegentEditorStyleKey::TabIcon))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FShuttlegentEditorModule::RegisterMenus));
	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FShuttlegentEditorModule::HandleEngineInitComplete);
	TabForegroundHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FShuttlegentEditorModule::HandleTabForegroundCheck), ShuttlegentEditorRuntime::TabForegroundCheckSeconds);
}

void FShuttlegentEditorModule::ShutdownModule()
{
	FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(ShuttlegentEditorSettingsUi::PropertyEditorModule);
	if (PropertyEditor != nullptr)
	{
		PropertyEditor->UnregisterCustomClassLayout(UShuttlegentSettings::StaticClass()->GetFName());
		PropertyEditor->UnregisterCustomPropertyTypeLayout(FShuttlegentModelEntry::StaticStruct()->GetFName());
		PropertyEditor->UnregisterCustomPropertyTypeLayout(FShuttlegentAgentEntry::StaticStruct()->GetFName());
	}

	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	FTSTicker::GetCoreTicker().RemoveTicker(PrewarmHandle);
	PrewarmHandle.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(TabForegroundHandle);
	TabForegroundHandle.Reset();

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized()) FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ShuttlegentEditorName::ChatTabId);
	Panel.Reset();

	if (ChatModel.IsValid()) ChatModel->Shutdown();
	ChatModel.Reset();

	FShuttlegentPanelCommands::Unregister();
	FShuttlegentStyle::Unregister();
}

TSharedRef<SDockTab> FShuttlegentEditorModule::SpawnChatTab(const FSpawnTabArgs& Args)
{
	// 模块生命周期内只有一个面板：重新打开的标签页显示同一页面，而不是重新加载
	if (!Panel.IsValid()) Panel = SNew(SShuttlegentWebPanel).Model(ChatModel);
	Panel->SetHostTabOpen(true);

	const TWeakPtr<SShuttlegentWebPanel> WeakPanel = Panel;
	const TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.TabColorScale_Raw(this, &FShuttlegentEditorModule::GetChatTabColor)
		.OnTabClosed_Lambda([WeakPanel](TSharedRef<SDockTab> ClosedTab)
		{
			const TSharedPtr<SShuttlegentWebPanel> Pinned = WeakPanel.Pin();
			if (Pinned.IsValid()) Pinned->SetHostTabOpen(false);
		})
		[
			Panel.ToSharedRef()
		];

	ChatTab = Tab;
	return Tab;
}

FLinearColor FShuttlegentEditorModule::GetChatTabColor() const
{
	// 透明即不画色条（SDockTab 只在不透明度大于 0 时绘制色条）
	const TSharedPtr<SDockTab> Tab = ChatTab.Pin();
	const EShuttlegentAttention Attention = ChatModel.IsValid() && Tab.IsValid() && !Tab->IsForeground() ? ChatModel->GetOverallAttention() : EShuttlegentAttention::None;
	if (Attention == EShuttlegentAttention::Input) return ShuttlegentEditorColor::AttentionInput;
	if (Attention == EShuttlegentAttention::Done) return ShuttlegentEditorColor::AttentionDone;
	return FLinearColor::Transparent;
}

bool FShuttlegentEditorModule::HandleTabForegroundCheck(float DeltaTime)
{
	const TSharedPtr<SDockTab> Tab = ChatTab.Pin();
	if (ChatModel.IsValid()) ChatModel->SetPanelVisible(Tab.IsValid() && Tab->IsForeground());
	return true;
}

void FShuttlegentEditorModule::HandleEngineInitComplete()
{
	// 稍作等待，让编辑器自身的启动工作先行；延迟结束时才读取该设置
	const TWeakPtr<FShuttlegentChatModel> WeakModel = ChatModel;
	PrewarmHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakModel](float DeltaTime)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->Prewarm();
		return false;
	}), ShuttlegentEditorRuntime::PrewarmDelaySeconds);
}

void FShuttlegentEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(ShuttlegentEditorName::WindowMenu);
	if (Menu == nullptr) return;

	FToolMenuSection& Section = Menu->FindOrAddSection(ShuttlegentEditorName::WindowMenuSection);
	Section.AddMenuEntry(
		ShuttlegentEditorName::ChatTabId,
		LOCTEXT("ChatMenuLabel", "Shuttlegent"),
		LOCTEXT("ChatMenuTooltip", "打开 Shuttlegent 面板"),
		FSlateIcon(ShuttlegentEditorName::StyleSetName, ShuttlegentEditorStyleKey::TabIcon),
		FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(ShuttlegentEditorName::ChatTabId); })));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FShuttlegentEditorModule, ShuttlegentEditor)
