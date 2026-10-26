#include "ShuttlegentStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentEditorConstants.h"

TSharedPtr<FSlateStyleSet> FShuttlegentStyle::StyleSet = nullptr;

void FShuttlegentStyle::Register()
{
	if (StyleSet.IsValid()) return;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
	const FString ResourcesDir = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), ShuttlegentEditorResource::ResourcesDir) : FString();
	const FString PngDir = FPaths::Combine(ResourcesDir, ShuttlegentEditorResource::PngDir);

	StyleSet = MakeShared<FSlateStyleSet>(ShuttlegentEditorName::StyleSetName);
	StyleSet->SetContentRoot(ResourcesDir);

	const FString MarkPath = FPaths::Combine(PngDir, FString(ShuttlegentEditorResource::MarkRedWhite) + ShuttlegentEditorResource::PngExtension);
	const FString TabIconPath = FPaths::Combine(PngDir, FString(ShuttlegentEditorResource::MarkReverse) + ShuttlegentEditorResource::PngExtension);
	const FVector2D MarkSize(ShuttlegentEditorMetric::MarkSizePlaceholder, ShuttlegentEditorMetric::MarkSizePlaceholder);
	const FVector2D TabIconSize(ShuttlegentEditorMetric::TabIconSize, ShuttlegentEditorMetric::TabIconSize);
	StyleSet->Set(ShuttlegentEditorStyleKey::Mark, new FSlateImageBrush(MarkPath, MarkSize));
	StyleSet->Set(ShuttlegentEditorStyleKey::TabIcon, new FSlateImageBrush(TabIconPath, TabIconSize));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FShuttlegentStyle::Unregister()
{
	if (!StyleSet.IsValid()) return;

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	StyleSet.Reset();
}

const ISlateStyle& FShuttlegentStyle::Get()
{
	check(StyleSet.IsValid());
	return *StyleSet;
}

const FSlateBrush* FShuttlegentStyle::Brush(const FName& Key)
{
	return Get().GetBrush(Key);
}
