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
}

void FShuttlegentEditorModule::ShutdownModule()
{
	FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(ShuttlegentEditorSettingsUi::PropertyEditorModule);
	if (PropertyEditor != nullptr) PropertyEditor->UnregisterCustomClassLayout(UShuttlegentSettings::StaticClass()->GetFName());

	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	FTSTicker::GetCoreTicker().RemoveTicker(PrewarmHandle);
	PrewarmHandle.Reset();

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
	// One panel for the module's lifetime: a reopened tab shows the same page instead of loading it again
	if (!Panel.IsValid()) Panel = SNew(SShuttlegentWebPanel).Model(ChatModel);
	Panel->SetHostTabOpen(true);

	const TWeakPtr<SShuttlegentWebPanel> WeakPanel = Panel;
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.OnTabClosed_Lambda([WeakPanel](TSharedRef<SDockTab> ClosedTab)
		{
			const TSharedPtr<SShuttlegentWebPanel> Pinned = WeakPanel.Pin();
			if (Pinned.IsValid()) Pinned->SetHostTabOpen(false);
		})
		[
			Panel.ToSharedRef()
		];
}

void FShuttlegentEditorModule::HandleEngineInitComplete()
{
	// Waits a moment so the editor's own startup work comes first; the setting is read when the delay ends
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
