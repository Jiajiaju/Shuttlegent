#include "Tools/FShuttlegentReflectionTools.h"

#include "Engine/DeveloperSettings.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#include "FShuttlegentToolBase.h"
#include "FShuttlegentToolObjects.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"

namespace FShuttlegentReflectionToolsPrivate
{
	// Tool names
	inline const TCHAR* GetPropertiesName = TEXT("get_properties");
	inline const TCHAR* DescribeClassName = TEXT("describe_class");
	inline const TCHAR* GetSettingsName = TEXT("get_settings");

	// Argument names
	inline const TCHAR* ArgPath = TEXT("path");
	inline const TCHAR* ArgNames = TEXT("names");
	inline const TCHAR* ArgAll = TEXT("all");
	inline const TCHAR* ArgClass = TEXT("class");
	inline const TCHAR* ArgDefaults = TEXT("defaults");
	inline const TCHAR* ArgOffset = TEXT("offset");
	inline const TCHAR* ArgLimit = TEXT("limit");

	/** Properties shown by default: the ones the Details panel shows. */
	bool IsEditable(const FProperty* Property)
	{
		return Property != nullptr && Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_Deprecated);
	}

	/** name (type) = value lines of an object's properties; names limits them to those, bAll includes non-editable ones. */
	TArray<FString> DescribeProperties(const UObject* Object, const TArray<FString>& Names, bool bAll, TArray<FString>& OutMissing)
	{
		TArray<FString> Lines;
		TSet<FString> Found;
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			const FString* Named = Names.FindByPredicate([Property](const FString& Wanted) { return FShuttlegentToolObjects::MatchesProperty(Property, Wanted); });
			if (!Names.IsEmpty() && Named == nullptr) continue;
			if (Names.IsEmpty() && !bAll && !IsEditable(Property)) continue;

			if (Named != nullptr) Found.Add(Named->ToLower());
			Lines.Add(FString::Printf(TEXT("%s = %s"), *FShuttlegentToolObjects::DescribeProperty(Property), *FShuttlegentToolObjects::ExportValue(Property, Object)));
		}

		for (const FString& Wanted : Names)
		{
			if (!Found.Contains(Wanted.ToLower())) OutMissing.Add(Wanted);
		}
		return Lines;
	}

	FString PageLines(const TArray<FString>& Lines, int32 Offset, int32 Limit)
	{
		TArray<FString> Page;
		const int32 End = FMath::Min(Offset + Limit, Lines.Num());
		for (int32 Index = Offset; Index < End; ++Index)
		{
			Page.Add(Lines[Index]);
		}
		return FShuttlegentToolArguments::MakePageHeader(Offset, Page.Num(), Lines.Num()) + TEXT("\n") + FString::Join(Page, TEXT("\n"));
	}

	class FGetProperties : public FShuttlegentToolBase
	{
	public:
		FGetProperties()
		{
			Definition.Name = GetPropertiesName;
			Definition.Description = TEXT("Property values of one object as editor text (the format set_property accepts): an actor, a component, an asset or a class default object. Each line is Name \"Details panel name\" [Category] (type) = value. Without names, lists the properties the Details panel shows. A user who names a setting in words means the Details panel name: match it on the object they selected (the actor) before looking at its components.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgPath, TEXT("Full object path, or the label of an actor in the open level. A component path is the actor path plus .ComponentName."), true)
				.StringArray(ArgNames, TEXT("Only these properties, by name or Details panel name."))
				.Boolean(ArgAll, TEXT("Include properties the Details panel hides. Defaults to false."))
				.Integer(ArgOffset, TEXT("Index of the first property, for paging. Defaults to 0."))
				.Integer(ArgLimit, TEXT("Properties per page, 1-200. Defaults to 50."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const FString Path = FShuttlegentToolArguments::GetString(Arguments, ArgPath);
			const UObject* Object = FShuttlegentToolObjects::ResolveObject(Path);
			if (!IsValid(Object)) return FShuttlegentToolResult::Error(FString::Printf(TEXT("No object found for %s."), *Path));

			int32 Offset = 0;
			int32 Limit = 0;
			FShuttlegentToolArguments::GetPage(Arguments, Offset, Limit);

			TArray<FString> Missing;
			const TArray<FString> Lines = DescribeProperties(Object, FShuttlegentToolArguments::GetStringArray(Arguments, ArgNames), FShuttlegentToolArguments::GetBoolean(Arguments, ArgAll, false), Missing);
			FString Text = FShuttlegentToolObjects::Describe(Object) + TEXT("\n") + PageLines(Lines, Offset, Limit);
			if (!Missing.IsEmpty()) Text += TEXT("\nNo such property: ") + FString::Join(Missing, TEXT(", "));
			return FShuttlegentToolResult::Ok(Text);
		}
	};

	class FDescribeClass : public FShuttlegentToolBase
	{
	public:
		FDescribeClass()
		{
			Definition.Name = DescribeClassName;
			Definition.Description = TEXT("A class's parent chain, editable properties with their types, and Blueprint-callable functions; optionally the class default values.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgClass, TEXT("Class name (StaticMeshActor), script path (/Script/Engine.StaticMeshActor) or Blueprint asset path."), true)
				.Boolean(ArgDefaults, TEXT("Show each property's class default value. Defaults to false."))
				.Integer(ArgOffset, TEXT("Index of the first line, for paging. Defaults to 0."))
				.Integer(ArgLimit, TEXT("Lines per page, 1-200. Defaults to 50."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const FString ClassName = FShuttlegentToolArguments::GetString(Arguments, ArgClass);
			UClass* Class = FShuttlegentToolObjects::ResolveClass(ClassName);
			if (!IsValid(Class)) return FShuttlegentToolResult::Error(FString::Printf(TEXT("Unknown class %s."), *ClassName));

			const bool bDefaults = FShuttlegentToolArguments::GetBoolean(Arguments, ArgDefaults, false);
			const UObject* Defaults = Class->GetDefaultObject();
			int32 Offset = 0;
			int32 Limit = 0;
			FShuttlegentToolArguments::GetPage(Arguments, Offset, Limit);

			TArray<FString> Parents;
			for (const UClass* Super = Class->GetSuperClass(); IsValid(Super); Super = Super->GetSuperClass())
			{
				Parents.Add(Super->GetName());
			}

			TArray<FString> Lines;
			for (TFieldIterator<FProperty> It(Class); It; ++It)
			{
				const FProperty* Property = *It;
				if (!IsEditable(Property)) continue;

				FString Line = TEXT("property ") + FShuttlegentToolObjects::DescribeProperty(Property);
				if (bDefaults) Line += TEXT(" = ") + FShuttlegentToolObjects::ExportValue(Property, Defaults);
				Lines.Add(Line);
			}
			for (TFieldIterator<UFunction> It(Class); It; ++It)
			{
				const UFunction* Function = *It;
				if (Function->HasAnyFunctionFlags(FUNC_BlueprintCallable)) Lines.Add(FString::Printf(TEXT("function %s"), *Function->GetName()));
			}

			const FString Header = FString::Printf(TEXT("%s (%s)\nParents: %s"), *Class->GetName(), *Class->GetPathName(), *FString::Join(Parents, TEXT(" > ")));
			return FShuttlegentToolResult::Ok(Header + TEXT("\n") + PageLines(Lines, Offset, Limit));
		}
	};

	class FGetSettings : public FShuttlegentToolBase
	{
	public:
		FGetSettings()
		{
			Definition.Name = GetSettingsName;
			Definition.Description = TEXT("Project settings and editor preferences pages (UDeveloperSettings). Without a class, lists the pages as class | container / category / section; with one, its current values.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgClass, TEXT("Settings class name, e.g. RendererSettings; empty lists the pages."))
				.StringArray(ArgNames, TEXT("Only these property names."))
				.Integer(ArgOffset, TEXT("Index of the first line, for paging. Defaults to 0."))
				.Integer(ArgLimit, TEXT("Lines per page, 1-200. Defaults to 50."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const FString ClassName = FShuttlegentToolArguments::GetString(Arguments, ArgClass);
			int32 Offset = 0;
			int32 Limit = 0;
			FShuttlegentToolArguments::GetPage(Arguments, Offset, Limit);

			if (ClassName.IsEmpty())
			{
				TArray<FString> Lines;
				for (TObjectIterator<UClass> It; It; ++It)
				{
					const UClass* Class = *It;
					if (!Class->IsChildOf(UDeveloperSettings::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract)) continue;

					const UDeveloperSettings* Settings = Cast<UDeveloperSettings>(Class->GetDefaultObject());
					if (IsValid(Settings)) Lines.Add(FString::Printf(TEXT("%s | %s / %s / %s"), *Class->GetName(), *Settings->GetContainerName().ToString(), *Settings->GetCategoryName().ToString(), *Settings->GetSectionName().ToString()));
				}
				Lines.Sort();
				return FShuttlegentToolResult::Ok(PageLines(Lines, Offset, Limit));
			}

			const UClass* Class = FShuttlegentToolObjects::ResolveClass(ClassName);
			if (!IsValid(Class) || !Class->IsChildOf(UDeveloperSettings::StaticClass())) return FShuttlegentToolResult::Error(FString::Printf(TEXT("%s is not a settings class; call get_settings without a class to list them."), *ClassName));

			TArray<FString> Missing;
			const TArray<FString> Lines = DescribeProperties(Class->GetDefaultObject(), FShuttlegentToolArguments::GetStringArray(Arguments, ArgNames), false, Missing);
			FString Text = FString::Printf(TEXT("%s (values in effect for this project and user)\n"), *Class->GetName()) + PageLines(Lines, Offset, Limit);
			if (!Missing.IsEmpty()) Text += TEXT("\nNo such property: ") + FString::Join(Missing, TEXT(", "));
			return FShuttlegentToolResult::Ok(Text);
		}
	};
}

void FShuttlegentReflectionTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentReflectionToolsPrivate::FGetProperties>());
	OutTools.Add(MakeShared<FShuttlegentReflectionToolsPrivate::FDescribeClass>());
	OutTools.Add(MakeShared<FShuttlegentReflectionToolsPrivate::FGetSettings>());
}
