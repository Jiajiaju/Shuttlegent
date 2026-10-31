#include "Endpoint/FShuttlegentExternalMcp.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"

#include "Endpoint/FShuttlegentExternalMcpState.h"
#include "FShuttlegentSecrets.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentMcpServerConstants.h"
#include "ShuttlegentProjectSettings.h"

namespace FShuttlegentExternalMcpPrivate
{
	// ACP stdio 条目的字段；stdio 条目没有 type 字段
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

	/** 说明服务器被剔除原因的日志文本。 */
	FString IssueText(const FShuttlegentExternalMcpState& State)
	{
		switch (State.Issue)
		{
		case EShuttlegentExternalMcpIssue::NameInvalid:
			{
				return TEXT("its name is empty or taken by the editor endpoint");
			}
		case EShuttlegentExternalMcpIssue::NameTaken:
			{
				return TEXT("another external server has the same name");
			}
		case EShuttlegentExternalMcpIssue::CommandNotAbsolute:
			{
				return TEXT("its command is not an absolute path");
			}
		case EShuttlegentExternalMcpIssue::UrlMissing:
			{
				return TEXT("it has no URL");
			}
		case EShuttlegentExternalMcpIssue::HeaderValueMissing:
			{
				return FString::Printf(TEXT("a header needs %s, which has no value"), *State.Detail);
			}
		case EShuttlegentExternalMcpIssue::None:
		default:
			{
				return FString();
			}
		}
	}

	/** 单个服务器的声明；被剔除时返回 nullptr，并在 OutState 中给出原因。 */
	TSharedPtr<FJsonObject> MakeDeclaration(const FShuttlegentExternalMcpServer& Server, FShuttlegentExternalMcpState& OutState)
	{
		if (Server.Name.IsEmpty() || Server.Name.Equals(ShuttlegentMcpEndpointConstant::ServerName, ESearchCase::IgnoreCase))
		{
			OutState.Issue = EShuttlegentExternalMcpIssue::NameInvalid;
			return nullptr;
		}

		const TSharedRef<FJsonObject> Declaration = MakeShared<FJsonObject>();
		Declaration->SetStringField(ShuttlegentMcpEndpointConstant::FieldName, Server.Name);
		if (Server.Type == EShuttlegentExternalMcpType::Stdio)
		{
			if (Server.Command.IsEmpty() || FPaths::IsRelative(Server.Command))
			{
				OutState.Issue = EShuttlegentExternalMcpIssue::CommandNotAbsolute;
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
			OutState.Issue = EShuttlegentExternalMcpIssue::UrlMissing;
			return nullptr;
		}

		TArray<TSharedPtr<FJsonValue>> Headers;
		for (const FShuttlegentMcpHeader& Header : Server.Headers)
		{
			FString Value;
			if (!Header.ValueVariable.IsEmpty() && !FShuttlegentSecrets::Resolve(Header.ValueVariable, Value))
			{
				OutState.Issue = EShuttlegentExternalMcpIssue::HeaderValueMissing;
				OutState.Detail = Header.ValueVariable;
				return nullptr;
			}
			Headers.Add(MakeShared<FJsonValueObject>(MakeNameValue(Header.Name, Header.Prefix + Value)));
		}
		Declaration->SetStringField(ShuttlegentMcpEndpointConstant::FieldType, ShuttlegentMcpEndpointConstant::DeclarationTypeHttp);
		Declaration->SetStringField(ShuttlegentMcpEndpointConstant::FieldUrl, Server.Url);
		Declaration->SetArrayField(ShuttlegentMcpEndpointConstant::FieldHeaders, Headers);
		return Declaration;
	}

	/** 可正常工作的服务器的声明，以及按设置顺序排列的全部服务器状态。 */
	void Evaluate(TArray<TSharedPtr<FJsonValue>>& OutDeclarations, TArray<FShuttlegentExternalMcpState>& OutStates)
	{
		const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
		if (!IsValid(Project)) return;

		TSet<FString> Names;
		for (const FShuttlegentExternalMcpServer& Server : Project->ExternalMcpServers)
		{
			FShuttlegentExternalMcpState& State = OutStates.AddDefaulted_GetRef();
			State.Name = Server.Name;
			State.bStdio = Server.Type == EShuttlegentExternalMcpType::Stdio;

			const TSharedPtr<FJsonObject> Declaration = MakeDeclaration(Server, State);
			if (Declaration.IsValid() && Names.Contains(Server.Name.ToLower())) State.Issue = EShuttlegentExternalMcpIssue::NameTaken;
			if (!Declaration.IsValid() || State.Issue != EShuttlegentExternalMcpIssue::None) continue;

			Names.Add(Server.Name.ToLower());
			OutDeclarations.Add(MakeShared<FJsonValueObject>(Declaration.ToSharedRef()));
		}
	}
}

TArray<TSharedPtr<FJsonValue>> FShuttlegentExternalMcp::MakeDeclarations()
{
	TArray<TSharedPtr<FJsonValue>> Declarations;
	TArray<FShuttlegentExternalMcpState> States;
	FShuttlegentExternalMcpPrivate::Evaluate(Declarations, States);

	for (const FShuttlegentExternalMcpState& State : States)
	{
		if (State.Issue != EShuttlegentExternalMcpIssue::None) UE_LOG(LogShuttlegent, Warning, TEXT("External MCP server '%s' left out: %s."), *State.Name, *FShuttlegentExternalMcpPrivate::IssueText(State));
	}
	return Declarations;
}

TArray<FShuttlegentExternalMcpState> FShuttlegentExternalMcp::DescribeServers()
{
	TArray<TSharedPtr<FJsonValue>> Declarations;
	TArray<FShuttlegentExternalMcpState> States;
	FShuttlegentExternalMcpPrivate::Evaluate(Declarations, States);
	return States;
}
