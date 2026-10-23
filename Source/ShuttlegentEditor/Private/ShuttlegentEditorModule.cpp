#include "ShuttlegentEditorModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#include "Chat/ShuttlegentChatModel.h"
#include "SShuttlegentChatPanel.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentCoreModule.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "ShuttlegentEditor"

void FShuttlegentEditorModule::StartupModule()
{
	FShuttlegentStyle::Register();

	ChatModel = MakeShared<FShuttlegentChatModel>();
	const TSharedPtr<FShuttlegentAgentService> Service = FShuttlegentCoreModule::Get().GetAgentService();
	if (Service.IsValid()) ChatModel->Initialize(Service.ToSharedRef());

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ShuttlegentEditorName::ChatTabId, FOnSpawnTab::CreateRaw(this, &FShuttlegentEditorModule::SpawnChatTab))
		.SetDisplayName(LOCTEXT("ChatTabTitle", "Shuttlegent"))
		.SetTooltipText(LOCTEXT("ChatTabTooltip", "在编辑器中与 AI Agent 协作"))
		.SetIcon(FSlateIcon(ShuttlegentEditorName::StyleSetName, ShuttlegentEditorStyleKey::TabIcon))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FShuttlegentEditorModule::RegisterMenus));
}

void FShuttlegentEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized()) FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ShuttlegentEditorName::ChatTabId);

	if (ChatModel.IsValid()) ChatModel->Shutdown();
	ChatModel.Reset();

	FShuttlegentStyle::Unregister();
}

TSharedRef<SDockTab> FShuttlegentEditorModule::SpawnChatTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SShuttlegentChatPanel)
			.Model(ChatModel)
		];
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
