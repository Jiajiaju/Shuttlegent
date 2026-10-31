#include "Reference/FShuttlegentReferenceNavigator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Docking/SDockTab.h"

#include "Reference/FShuttlegentReference.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentLog.h"

namespace FShuttlegentReferenceNavigatorPrivate
{
	bool RevealAsset(const FShuttlegentReference& Reference, bool bOpen)
	{
		FAssetRegistryModule* RegistryModule = FModuleManager::LoadModulePtr<FAssetRegistryModule>(ShuttlegentEditorReference::AssetRegistryModule);
		if (RegistryModule == nullptr) return false;

		const FAssetData Asset = RegistryModule->Get().GetAssetByObjectPath(FSoftObjectPath(Reference.Path));
		if (!Asset.IsValid()) return false;

		FContentBrowserModule* ContentBrowserModule = FModuleManager::LoadModulePtr<FContentBrowserModule>(ShuttlegentEditorWeb::ContentBrowserModule);
		if (ContentBrowserModule != nullptr) ContentBrowserModule->Get().SyncBrowserToAssets(TArray<FAssetData>({ Asset }));

		if (!bOpen) return true;

		UObject* Object = Asset.GetAsset();
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor != nullptr ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
		if (!IsValid(Object) || !IsValid(AssetEditorSubsystem)) return false;

		return AssetEditorSubsystem->OpenEditorForAsset(Object);
	}

	/** 把关卡编辑器页签切到前台（面板与它同处一个页签栏时，选中的 Actor 才看得见），并让视口获得键盘焦点。 */
	void ShowLevelEditor()
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(ShuttlegentEditorReference::LevelEditorModule);
		if (LevelEditorModule == nullptr) return;

		const TSharedPtr<SDockTab> LevelEditorTab = LevelEditorModule->GetLevelEditorInstanceTab().Pin();
		if (LevelEditorTab.IsValid()) LevelEditorTab->DrawAttention();

		LevelEditorModule->FocusViewport();
	}

	bool RevealActor(const FShuttlegentReference& Reference, bool bOpen)
	{
		AActor* Actor = Cast<AActor>(FSoftObjectPath(Reference.Path).ResolveObject());
		if (!IsValid(Actor) || GEditor == nullptr) return false;

		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Actor, true, true);
		ShowLevelEditor();

		if (bOpen) GEditor->MoveViewportCamerasToActor(*Actor, false);
		return true;
	}
}

bool FShuttlegentReferenceNavigator::Reveal(const FShuttlegentReference& Reference, bool bOpen)
{
	const bool bActor = Reference.Kind == EShuttlegentReferenceKind::Actor;
	const bool bRevealed = bActor ? FShuttlegentReferenceNavigatorPrivate::RevealActor(Reference, bOpen) : FShuttlegentReferenceNavigatorPrivate::RevealAsset(Reference, bOpen);

	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent panel: %s %s %s%s"), bOpen ? TEXT("open") : TEXT("reveal"), bActor ? TEXT("actor") : TEXT("asset"), *Reference.Path, bRevealed ? TEXT("") : TEXT(" (not found)"));
	return bRevealed;
}
