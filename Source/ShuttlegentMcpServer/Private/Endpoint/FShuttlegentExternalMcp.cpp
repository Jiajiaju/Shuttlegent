#include "Endpoint/FShuttlegentExternalMcp.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"

#include "FShuttlegentSecrets.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentMcpServerConstants.h"
#include "ShuttlegentProjectSettings.h"

namespace FShuttlegentExternalMcpPrivate
{
	// ACP stdio entry fields; a stdio entry has no type field
	inline const TCHAR* FieldCommand = TEXT("command");
	inline const TCHAR* FieldArgs = TEXT("args");
	inline const TCHAR* FieldEnv = TEXT("env");

	TSharedRef<FJsonObject> MakeNameValue(const FString& Name, const FString& Value)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(ShuttlegentMcpEndpointConstant::FieldName, Name);
		Entry->SetStringField(ShuttlegentMcpEndpointConstant::FieldValue, Value);
		return Entry;
	}

	/** The declaration of one server, or nullptr with the reason it is left out. */
	TSharedPtr<FJsonObject> MakeDeclaration(const FShuttlegentExternalMcpServer& Server, FString& OutReason)
	{
		if (Server.Name.IsEmpty() || Server.Name.Equals(ShuttlegentMcpEndpointConstant::ServerName, ESearchCase::IgnoreCase))
		{
			OutReason = TEXT("its name is empty or taken by the editor endpoint");
			return nullptr;
		}

		const TSharedRef<FJsonObject> Declaration = MakeShared<FJsonObject>();
		Declaration->SetStringField(ShuttlegentMcpEndpointConstant::FieldName, Server.Name);
		if (Server.Type == EShuttlegentExternalMcpType::Stdio)
		{
			if (Server.Command.IsEmpty() || FPaths::IsRelative(Server.Command))
			{
				OutReason = TEXT("its command is not an absolute path");
				return nullptr;
			}

			TArray<TSharedPtr<FJsonValue>> Arguments;
			for (const FString& Argument : Server.Arguments)
			{
				Arguments.Add(MakeShared<FJsonValueString>(Argument));
			}
			Declaration->SetStringField(FieldCommand, Server.Command);
			Declaration->SetArrayField(FieldArgs, Arguments);
			Declaration->SetArrayField(FieldEnv, TArray<TSharedPtr<FJsonValue>>());
			return Declaration;
		}

		if (Server.Url.IsEmpty())
		{
			OutReason = TEXT("it has no URL");
			return nullptr;
		}

		TArray<TSharedPtr<FJsonValue>> Headers;
		for (const FShuttlegentMcpHeader& Header : Server.Headers)
		{
			FString Value;
			EShuttlegentSecretSource Source = EShuttlegentSecretSource::None;
			if (!Header.ValueVariable.IsEmpty() && !FShuttlegentSecrets::Resolve(Header.ValueVariable, Value, Source))
			{
				OutReason = FString::Printf(TEXT("header %s needs %s, which has no value"), *Header.Name, *Header.ValueVariable);
				return nullptr;
			}
			Headers.Add(MakeShared<FJsonValueObject>(MakeNameValue(Header.Name, Header.Prefix + Value)));
		}
		Declaration->SetStringField(ShuttlegentMcpEndpointConstant::FieldType, ShuttlegentMcpEndpointConstant::DeclarationTypeHttp);
		Declaration->SetStringField(ShuttlegentMcpEndpointConstant::FieldUrl, Server.Url);
		Declaration->SetArrayField(ShuttlegentMcpEndpointConstant::FieldHeaders, Headers);
		return Declaration;
	}
}

TArray<TSharedPtr<FJsonValue>> FShuttlegentExternalMcp::MakeDeclarations()
{
	TArray<TSharedPtr<FJsonValue>> Declarations;
	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	if (!IsValid(Project)) return Declarations;

	TSet<FString> Names;
	for (const FShuttlegentExternalMcpServer& Server : Project->ExternalMcpServers)
	{
		FString Reason;
		const TSharedPtr<FJsonObject> Declaration = FShuttlegentExternalMcpPrivate::MakeDeclaration(Server, Reason);
		if (Declaration.IsValid() && Names.Contains(Server.Name.ToLower())) Reason = TEXT("another external server has the same name");
		if (!Declaration.IsValid() || !Reason.IsEmpty())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("External MCP server '%s' left out: %s."), *Server.Name, *Reason);
			continue;
		}

		Names.Add(Server.Name.ToLower());
		Declarations.Add(MakeShared<FJsonValueObject>(Declaration.ToSharedRef()));
	}
	return Declarations;
}
