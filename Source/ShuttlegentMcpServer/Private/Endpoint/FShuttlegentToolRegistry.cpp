#include "Endpoint/FShuttlegentToolRegistry.h"

#include "Features/IModularFeatures.h"
#include "Internationalization/Regex.h"

#include "ShuttlegentLog.h"
#include "ShuttlegentMcpServerConstants.h"
#include "Tool/IShuttlegentToolProvider.h"

TArray<TSharedRef<IShuttlegentTool>> FShuttlegentToolRegistry::Collect()
{
	TArray<TSharedRef<IShuttlegentTool>> Tools;
	TSet<FString> Names;
	const FRegexPattern NamePattern(ShuttlegentMcpTool::NamePattern);

	IModularFeatures& Features = IModularFeatures::Get();
	const TArray<IShuttlegentToolProvider*> Providers = Features.GetModularFeatureImplementations<IShuttlegentToolProvider>(IShuttlegentToolProvider::GetModularFeatureName());
	for (const IShuttlegentToolProvider* Provider : Providers)
	{
		TArray<TSharedRef<IShuttlegentTool>> Provided;
		Provider->GetTools(Provided);
		for (const TSharedRef<IShuttlegentTool>& Tool : Provided)
		{
			const FString& Name = Tool->GetDefinition().Name;
			FRegexMatcher Matcher(NamePattern, Name);
			if (!Matcher.FindNext())
			{
				UE_LOG(LogShuttlegent, Warning, TEXT("Tool '%s' skipped: names use lowercase letters, digits and underscores only."), *Name);
				continue;
			}
			if (Names.Contains(Name))
			{
				UE_LOG(LogShuttlegent, Warning, TEXT("Tool '%s' skipped: another provider already offers a tool of that name."), *Name);
				continue;
			}

			Names.Add(Name);
			Tools.Add(Tool);
		}
	}
	return Tools;
}

TSharedPtr<IShuttlegentTool> FShuttlegentToolRegistry::Find(const FString& Name)
{
	for (const TSharedRef<IShuttlegentTool>& Tool : Collect())
	{
		if (Tool->GetDefinition().Name == Name) return Tool;
	}
	return nullptr;
}
