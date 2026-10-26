#include "Tools/FShuttlegentAssetTools.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "FShuttlegentToolBase.h"
#include "ShuttlegentToolsConstants.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"

namespace FShuttlegentAssetToolsPrivate
{
	// Tool names
	inline const TCHAR* FindAssetsName = TEXT("find_assets");
	inline const TCHAR* GetAssetName = TEXT("get_asset");
	inline const TCHAR* GetReferencesName = TEXT("get_asset_references");

	// Argument names
	inline const TCHAR* ArgQuery = TEXT("query");
	inline const TCHAR* ArgClass = TEXT("class");
	inline const TCHAR* ArgPath = TEXT("path");
	inline const TCHAR* ArgRecursive = TEXT("recursive");
	inline const TCHAR* ArgDirection = TEXT("direction");
	inline const TCHAR* ArgOffset = TEXT("offset");
	inline const TCHAR* ArgLimit = TEXT("limit");

	// get_asset_references directions
	inline const TCHAR* DirectionDependencies = TEXT("dependencies");
	inline const TCHAR* DirectionReferencers = TEXT("referencers");

	IAssetRegistry& GetRegistry()
	{
		return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();
	}

	/** The asset an object path or a package name names; invalid when the registry has none. */
	FAssetData FindAsset(const FString& Path)
	{
		IAssetRegistry& Registry = GetRegistry();
		FAssetData Asset = Registry.GetAssetByObjectPath(FSoftObjectPath(Path));
		if (Asset.IsValid()) return Asset;

		TArray<FAssetData> InPackage;
		Registry.GetAssetsByPackageName(FName(*Path), InPackage);
		return InPackage.IsEmpty() ? FAssetData() : InPackage[0];
	}

	FString DescribeAsset(const FAssetData& Asset)
	{
		return FString::Printf(TEXT("%s | %s | %s"), *Asset.AssetName.ToString(), *Asset.AssetClassPath.GetAssetName().ToString(), *Asset.GetObjectPathString());
	}

	class FFindAssets : public FShuttlegentToolBase
	{
	public:
		FFindAssets()
		{
			Definition.Name = FindAssetsName;
			Definition.Description = TEXT("Find assets in the asset registry by name, class and folder, without loading them. Lists name | class | object path, sorted by name.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgQuery, TEXT("Part of the asset name, case-insensitive; empty matches all."))
				.String(ArgClass, TEXT("Asset class name, e.g. StaticMesh, Material, Blueprint, World; empty matches all."))
				.String(ArgPath, TEXT("Package folder to search, e.g. /Game/Maps. Defaults to /Game."))
				.Boolean(ArgRecursive, TEXT("Search sub-folders too. Defaults to true."))
				.Integer(ArgOffset, TEXT("Index of the first result, for paging. Defaults to 0."))
				.Integer(ArgLimit, TEXT("Results per page, 1-200. Defaults to 50."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const FString Query = FShuttlegentToolArguments::GetString(Arguments, ArgQuery);
			const FString ClassName = FShuttlegentToolArguments::GetString(Arguments, ArgClass);
			const FString Path = FShuttlegentToolArguments::GetString(Arguments, ArgPath, ShuttlegentToolsConstant::DefaultAssetRoot);
			int32 Offset = 0;
			int32 Limit = 0;
			FShuttlegentToolArguments::GetPage(Arguments, Offset, Limit);

			FARFilter Filter;
			Filter.PackagePaths.Add(FName(*Path));
			Filter.bRecursivePaths = FShuttlegentToolArguments::GetBoolean(Arguments, ArgRecursive, true);
			TArray<FAssetData> Assets;
			GetRegistry().GetAssets(Filter, Assets);

			Assets.RemoveAll([&Query, &ClassName](const FAssetData& Asset)
			{
				const bool bNameMatches = Query.IsEmpty() || Asset.AssetName.ToString().Contains(Query);
				const bool bClassMatches = ClassName.IsEmpty() || Asset.AssetClassPath.GetAssetName().ToString().Equals(ClassName, ESearchCase::IgnoreCase);
				return !bNameMatches || !bClassMatches;
			});
			Assets.Sort([](const FAssetData& Left, const FAssetData& Right) { return Left.AssetName.LexicalLess(Right.AssetName); });

			TArray<FString> Lines;
			const int32 End = FMath::Min(Offset + Limit, Assets.Num());
			for (int32 Index = Offset; Index < End; ++Index)
			{
				Lines.Add(DescribeAsset(Assets[Index]));
			}
			return FShuttlegentToolResult::Ok(FShuttlegentToolArguments::MakePageHeader(Offset, Lines.Num(), Assets.Num()) + TEXT("\n") + FString::Join(Lines, TEXT("\n")));
		}
	};

	class FGetAsset : public FShuttlegentToolBase
	{
	public:
		FGetAsset()
		{
			Definition.Name = GetAssetName;
			Definition.Description = TEXT("Details of one asset from the asset registry, without loading it: class, package, registry tags, and how many packages it depends on and are referencing it.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgPath, TEXT("Object path (/Game/Props/SM_Box.SM_Box) or package name (/Game/Props/SM_Box)."), true)
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const FString Path = FShuttlegentToolArguments::GetString(Arguments, ArgPath);
			const FAssetData Asset = FindAsset(Path);
			if (!Asset.IsValid()) return FShuttlegentToolResult::Error(FString::Printf(TEXT("No asset found at %s."), *Path));

			IAssetRegistry& Registry = GetRegistry();
			TArray<FName> Dependencies;
			TArray<FName> Referencers;
			Registry.GetDependencies(Asset.PackageName, Dependencies);
			Registry.GetReferencers(Asset.PackageName, Referencers);

			TArray<FString> Lines;
			Lines.Add(FString::Printf(TEXT("Name: %s"), *Asset.AssetName.ToString()));
			Lines.Add(FString::Printf(TEXT("Class: %s"), *Asset.AssetClassPath.ToString()));
			Lines.Add(FString::Printf(TEXT("Object path: %s"), *Asset.GetObjectPathString()));
			Lines.Add(FString::Printf(TEXT("Package: %s"), *Asset.PackageName.ToString()));
			Lines.Add(FString::Printf(TEXT("Loaded: %s"), Asset.IsAssetLoaded() ? TEXT("yes") : TEXT("no")));
			Lines.Add(FString::Printf(TEXT("Depends on %d package(s); referenced by %d package(s)."), Dependencies.Num(), Referencers.Num()));

			Lines.Add(TEXT("Tags:"));
			int32 TagCount = 0;
			for (const TPair<FName, FAssetTagValueRef>& Tag : Asset.TagsAndValues)
			{
				if (TagCount++ >= ShuttlegentToolsConstant::MaxAssetTags)
				{
					Lines.Add(FString::Printf(TEXT("- (%d more not shown)"), Asset.TagsAndValues.Num() - ShuttlegentToolsConstant::MaxAssetTags));
					break;
				}
				Lines.Add(FString::Printf(TEXT("- %s = %s"), *Tag.Key.ToString(), *Tag.Value.AsString().Left(ShuttlegentToolsConstant::MaxValueChars)));
			}
			return FShuttlegentToolResult::Ok(FString::Join(Lines, TEXT("\n")));
		}
	};

	class FGetAssetReferences : public FShuttlegentToolBase
	{
	public:
		FGetAssetReferences()
		{
			Definition.Name = GetReferencesName;
			Definition.Description = TEXT("Packages an asset depends on, or packages that reference it, from the asset registry. Lists package names, sorted.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgPath, TEXT("Object path or package name of the asset."), true)
				.Enum(ArgDirection, TEXT("dependencies: what the asset uses; referencers: what uses the asset."), { DirectionDependencies, DirectionReferencers }, true)
				.Integer(ArgOffset, TEXT("Index of the first result, for paging. Defaults to 0."))
				.Integer(ArgLimit, TEXT("Results per page, 1-200. Defaults to 50."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const FString Path = FShuttlegentToolArguments::GetString(Arguments, ArgPath);
			const FString Direction = FShuttlegentToolArguments::GetString(Arguments, ArgDirection);
			const FAssetData Asset = FindAsset(Path);
			if (!Asset.IsValid()) return FShuttlegentToolResult::Error(FString::Printf(TEXT("No asset found at %s."), *Path));
			if (Direction != DirectionDependencies && Direction != DirectionReferencers) return FShuttlegentToolResult::Error(TEXT("direction must be dependencies or referencers."));

			int32 Offset = 0;
			int32 Limit = 0;
			FShuttlegentToolArguments::GetPage(Arguments, Offset, Limit);

			TArray<FName> Packages;
			if (Direction == DirectionDependencies) GetRegistry().GetDependencies(Asset.PackageName, Packages);
			else GetRegistry().GetReferencers(Asset.PackageName, Packages);
			Packages.Sort(FNameLexicalLess());

			TArray<FString> Lines;
			const int32 End = FMath::Min(Offset + Limit, Packages.Num());
			for (int32 Index = Offset; Index < End; ++Index)
			{
				Lines.Add(Packages[Index].ToString());
			}
			return FShuttlegentToolResult::Ok(FShuttlegentToolArguments::MakePageHeader(Offset, Lines.Num(), Packages.Num()) + TEXT("\n") + FString::Join(Lines, TEXT("\n")));
		}
	};
}

void FShuttlegentAssetTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentAssetToolsPrivate::FFindAssets>());
	OutTools.Add(MakeShared<FShuttlegentAssetToolsPrivate::FGetAsset>());
	OutTools.Add(MakeShared<FShuttlegentAssetToolsPrivate::FGetAssetReferences>());
}
