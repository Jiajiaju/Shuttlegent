#include "Tool/FShuttlegentToolSchema.h"

#include "Dom/JsonValue.h"

#include "ShuttlegentMcpServerConstants.h"

namespace FShuttlegentToolSchemaPrivate
{
	TSharedRef<FJsonObject> MakeProperty(const TCHAR* Type, const FString& Description)
	{
		const TSharedRef<FJsonObject> Property = MakeShared<FJsonObject>();
		Property->SetStringField(ShuttlegentMcpTool::SchemaType, Type);
		Property->SetStringField(ShuttlegentMcpTool::SchemaDescription, Description);
		return Property;
	}

	TSharedRef<FJsonObject> MakeItems(const TCHAR* Type)
	{
		const TSharedRef<FJsonObject> Items = MakeShared<FJsonObject>();
		Items->SetStringField(ShuttlegentMcpTool::SchemaType, Type);
		return Items;
	}
}

FShuttlegentToolSchema& FShuttlegentToolSchema::String(const FString& Name, const FString& Description, bool bRequired)
{
	return Add(Name, FShuttlegentToolSchemaPrivate::MakeProperty(ShuttlegentMcpTool::TypeString, Description), bRequired);
}

FShuttlegentToolSchema& FShuttlegentToolSchema::Integer(const FString& Name, const FString& Description, bool bRequired)
{
	return Add(Name, FShuttlegentToolSchemaPrivate::MakeProperty(ShuttlegentMcpTool::TypeInteger, Description), bRequired);
}

FShuttlegentToolSchema& FShuttlegentToolSchema::Number(const FString& Name, const FString& Description, bool bRequired)
{
	return Add(Name, FShuttlegentToolSchemaPrivate::MakeProperty(ShuttlegentMcpTool::TypeNumber, Description), bRequired);
}

FShuttlegentToolSchema& FShuttlegentToolSchema::Boolean(const FString& Name, const FString& Description, bool bRequired)
{
	return Add(Name, FShuttlegentToolSchemaPrivate::MakeProperty(ShuttlegentMcpTool::TypeBoolean, Description), bRequired);
}

FShuttlegentToolSchema& FShuttlegentToolSchema::StringArray(const FString& Name, const FString& Description, bool bRequired)
{
	const TSharedRef<FJsonObject> Property = FShuttlegentToolSchemaPrivate::MakeProperty(ShuttlegentMcpTool::TypeArray, Description);
	Property->SetObjectField(ShuttlegentMcpTool::SchemaItems, FShuttlegentToolSchemaPrivate::MakeItems(ShuttlegentMcpTool::TypeString));
	return Add(Name, Property, bRequired);
}

FShuttlegentToolSchema& FShuttlegentToolSchema::Object(const FString& Name, const FString& Description, bool bRequired)
{
	return Add(Name, FShuttlegentToolSchemaPrivate::MakeProperty(ShuttlegentMcpTool::TypeObject, Description), bRequired);
}

FShuttlegentToolSchema& FShuttlegentToolSchema::Enum(const FString& Name, const FString& Description, const TArray<FString>& Values, bool bRequired)
{
	TArray<TSharedPtr<FJsonValue>> EnumValues;
	for (const FString& Value : Values)
	{
		EnumValues.Add(MakeShared<FJsonValueString>(Value));
	}

	const TSharedRef<FJsonObject> Property = FShuttlegentToolSchemaPrivate::MakeProperty(ShuttlegentMcpTool::TypeString, Description);
	Property->SetArrayField(ShuttlegentMcpTool::SchemaEnum, EnumValues);
	return Add(Name, Property, bRequired);
}

FShuttlegentToolSchema& FShuttlegentToolSchema::NumberTuple(const FString& Name, const FString& Description, int32 Length, bool bRequired)
{
	const TSharedRef<FJsonObject> Property = FShuttlegentToolSchemaPrivate::MakeProperty(ShuttlegentMcpTool::TypeArray, Description);
	Property->SetObjectField(ShuttlegentMcpTool::SchemaItems, FShuttlegentToolSchemaPrivate::MakeItems(ShuttlegentMcpTool::TypeNumber));
	Property->SetNumberField(ShuttlegentMcpTool::SchemaMinItems, Length);
	Property->SetNumberField(ShuttlegentMcpTool::SchemaMaxItems, Length);
	return Add(Name, Property, bRequired);
}

TSharedPtr<FJsonObject> FShuttlegentToolSchema::Build() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(ShuttlegentMcpTool::SchemaType, ShuttlegentMcpTool::TypeObject);
	Root->SetObjectField(ShuttlegentMcpTool::SchemaProperties, Properties);
	if (!Required.IsEmpty()) Root->SetArrayField(ShuttlegentMcpTool::SchemaRequired, Required);
	Root->SetBoolField(ShuttlegentMcpTool::SchemaAdditionalProperties, false);
	return Root;
}

FShuttlegentToolSchema& FShuttlegentToolSchema::Add(const FString& Name, const TSharedRef<FJsonObject>& Property, bool bRequired)
{
	Properties->SetObjectField(Name, Property);
	if (bRequired) Required.Add(MakeShared<FJsonValueString>(Name));
	return *this;
}
