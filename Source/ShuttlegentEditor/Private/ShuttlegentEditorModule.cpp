#include "ShuttlegentEditorModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/CoreDelegates.h"
#include "Misc/MessageDialog.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#include "Chat/FShuttlegentChatView.h"
#include "Chat/ShuttlegentChatModel.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentCoreModule.h"
#include "FShuttlegentAgentEntry.h"
#include "FShuttlegentModelEntry.h"
#include "Settings/FShuttlegentAgentEntryCustomization.h"
#include "Settings/FShuttlegentModelEntryCustomization.h"
#include "Settings/FShuttlegentSettingsCustomization.h"
#include "Tabs/FShuttlegentSessionTabRegistry.h"
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
	HubView = MakeShared<FShuttlegentChatView>();
	HubView->Initialize(ChatModel.ToSharedRef(), EShuttlegentViewMode::Hub);
	SessionTabs = MakeShared<FShuttlegentSessionTabRegistry>();
	SessionTabs->Initialize(ChatModel.ToSharedRef(), HubView.ToSharedRef());
	// 主页签显示上次的当前会话；会话页签由关卡编辑器恢复布局时接回
	SessionTabs->RestoreHub();

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
	IMainFrameModule* MainFrame = FModuleManager::GetModulePtr<IMainFrameModule>(ShuttlegentEditorRuntime::MainFrameModule);
	if (MainFrame != nullptr && CanCloseEditorHandle.IsValid()) MainFrame->UnregisterCanCloseEditor(CanCloseEditorHandle);
	CanCloseEditorHandle.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(PrewarmHandle);
	PrewarmHandle.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(TabForegroundHandle);
	TabForegroundHandle.Reset();

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized()) FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ShuttlegentEditorName::ChatTabId);
	Panel.Reset();

	if (SessionTabs.IsValid()) SessionTabs->Shutdown();
	SessionTabs.Reset();
	if (HubView.IsValid()) HubView->Shutdown();
	HubView.Reset();
	if (ChatModel.IsValid()) ChatModel->Shutdown();
	ChatModel.Reset();

	FShuttlegentPanelCommands::Unregister();
	FShuttlegentStyle::Unregister();
}

TSharedRef<SDockTab> FShuttlegentEditorModule::SpawnChatTab(const FSpawnTabArgs& Args)
{
	// 模块生命周期内只有一个面板：重新打开的标签页显示同一页面，而不是重新加载
	if (!Panel.IsValid())
	{
		const TWeakPtr<FShuttlegentSessionTabRegistry> WeakTabs = SessionTabs;
		Panel = SNew(SShuttlegentWebPanel)
			.View(HubView)
			.BeginSessionDrag([WeakTabs](const TSharedPtr<SDockTab>& HostTab, const FString& SessionId)
			{
				const TSharedPtr<FShuttlegentSessionTabRegistry> Tabs = WeakTabs.Pin();
				// 会话窗口开在光标处，与宿主页签所在的位置无关
				if (Tabs.IsValid()) Tabs->BeginDrag(SessionId);
			})
			.FocusSession([WeakTabs](const FString& SessionId)
			{
				const TSharedPtr<FShuttlegentSessionTabRegistry> Tabs = WeakTabs.Pin();
				return Tabs.IsValid() && Tabs->FocusSession(SessionId);
			});
	}
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
	Panel->SetHostTab(Tab);
	return Tab;
}

FLinearColor FShuttlegentEditorModule::GetChatTabColor() const
{
	// 透明即不画色条（SDockTab 只在不透明度大于 0 时绘制色条）
	const TSharedPtr<SDockTab> Tab = ChatTab.Pin();
	// 会话页签里的会话由那个页签自己提醒
	const EShuttlegentAttention Attention = HubView.IsValid() && Tab.IsValid() && !Tab->IsForeground() ? HubView->GetTabAttention() : EShuttlegentAttention::None;
	return FShuttlegentSessionTabRegistry::GetAttentionColor(Attention);
}

bool FShuttlegentEditorModule::HandleTabForegroundCheck(float DeltaTime)
{
	const TSharedPtr<SDockTab> Tab = ChatTab.Pin();
	if (HubView.IsValid()) HubView->SetVisible(Tab.IsValid() && Tab->IsForeground());
	if (SessionTabs.IsValid()) SessionTabs->UpdateVisibility();
	return true;
}

bool FShuttlegentEditorModule::HandleCanCloseEditor()
{
	if (!ChatModel.IsValid()) return true;

	const TArray<FString> Titles = ChatModel->GetBusyTitles();
	if (Titles.IsEmpty()) return true;

	FString List;
	for (const FString& Title : Titles)
	{
		List += FString::Printf(TEXT("\n  · %s"), *Title);
	}
	const FText Message = FText::Format(LOCTEXT("CloseDuringTurn", "以下会话的回合仍在进行，关闭编辑器会中断它们：{0}\n\n未确认的写入、权限请求与提问将作废；重新打开后可在对话末尾点“重试”。仍要关闭吗？"), FText::FromString(List));
	if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::No, Message, LOCTEXT("CloseDuringTurnTitle", "Shuttlegent 回合进行中")) != EAppReturnType::Yes) return false;

	ChatModel->SettleBeforeExit();
	return true;
}

void FShuttlegentEditorModule::HandleEngineInitComplete()
{
	// 主框架此时已加载；关闭编辑器前有回合在进行时先确认
	IMainFrameModule* MainFrame = FModuleManager::GetModulePtr<IMainFrameModule>(ShuttlegentEditorRuntime::MainFrameModule);
	if (MainFrame != nullptr) CanCloseEditorHandle = MainFrame->RegisterCanCloseEditor(IMainFrameModule::FMainFrameCanCloseEditor::CreateRaw(this, &FShuttlegentEditorModule::HandleCanCloseEditor));

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
