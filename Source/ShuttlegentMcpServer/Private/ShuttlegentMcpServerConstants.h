#pragma once

#include "CoreMinimal.h"

/** Editor write gate timing (Shuttlegent.md section 10.3). */
namespace ShuttlegentWriteGateConstant
{
	// A request waits this long for confirmation; under the runtime's 60 s MCP tool call timeout, so the gate answers first
	inline constexpr double ConfirmTimeoutSeconds = 55.0;

	// How often waiting requests are checked for the timeout, in seconds
	inline constexpr float TimeoutCheckSeconds = 0.5f;
}

/** Shuttlegent.WriteProbe: an editor write through the write gate, for acceptance before the MCP endpoint exists (Shuttlegent.md section 14.3.6). */
namespace ShuttlegentWriteProbe
{
	inline const TCHAR* CommandName = TEXT("Shuttlegent.WriteProbe");
	inline const TCHAR* CommandHelp = TEXT("Ask through the write gate to move the selected actors up: Shuttlegent.WriteProbe [ZOffsetCm=100] [FailFromIndex=-1] [DelaySeconds=0]. FailFromIndex makes that actor fail and leaves the rest unrun; DelaySeconds waits that long after confirmation before writing, so a write can be in flight when the runtime is lost.");

	// Tool name shown on the confirmation card, as an MCP tool would be named
	inline const TCHAR* ToolName = TEXT("shuttlegent_write_probe");

	// Argument positions and defaults
	inline constexpr int32 ArgZOffset = 0;
	inline constexpr int32 ArgFailFrom = 1;
	inline constexpr int32 ArgDelay = 2;
	inline constexpr float DefaultZOffset = 100.0f;
	inline constexpr int32 NoFailure = -1;
	inline constexpr float NoDelay = 0.0f;

	// Raw argument fields
	inline const TCHAR* FieldTool = TEXT("tool");
	inline const TCHAR* FieldZOffset = TEXT("zOffsetCm");
	inline const TCHAR* FieldFailFrom = TEXT("failFromIndex");
	inline const TCHAR* FieldDelay = TEXT("delaySeconds");
	inline const TCHAR* FieldActors = TEXT("actors");

	// Shown values: Z in cm with this many decimals
	inline constexpr int32 ValueFractionalDigits = 1;
}

/** Tool arguments, schemas and output limits (Shuttlegent.md section 10.2). */
namespace ShuttlegentMcpTool
{
	// Paging arguments of list tools and their limits
	inline const TCHAR* ArgOffset = TEXT("offset");
	inline const TCHAR* ArgLimit = TEXT("limit");
	inline constexpr int32 DefaultPageLimit = 50;
	inline constexpr int32 MaxPageLimit = 200;

	// Longest text one call returns, in characters; below the runtime's 12500-token spill threshold
	inline constexpr int32 MaxOutputChars = 24000;

	// JSON Schema keywords and types
	inline const TCHAR* SchemaType = TEXT("type");
	inline const TCHAR* SchemaProperties = TEXT("properties");
	inline const TCHAR* SchemaRequired = TEXT("required");
	inline const TCHAR* SchemaDescription = TEXT("description");
	inline const TCHAR* SchemaItems = TEXT("items");
	inline const TCHAR* SchemaEnum = TEXT("enum");
	inline const TCHAR* SchemaMinItems = TEXT("minItems");
	inline const TCHAR* SchemaMaxItems = TEXT("maxItems");
	inline const TCHAR* SchemaAdditionalProperties = TEXT("additionalProperties");
	inline const TCHAR* TypeObject = TEXT("object");
	inline const TCHAR* TypeString = TEXT("string");
	inline const TCHAR* TypeInteger = TEXT("integer");
	inline const TCHAR* TypeNumber = TEXT("number");
	inline const TCHAR* TypeBoolean = TEXT("boolean");
	inline const TCHAR* TypeArray = TEXT("array");

	// Tool names: lowercase letters, digits and underscores
	inline const TCHAR* NamePattern = TEXT("^[a-z0-9_]+$");

	// IModularFeatures name of IShuttlegentToolProvider
	inline const TCHAR* ProviderFeatureName = TEXT("ShuttlegentToolProvider");
}

/** The editor MCP endpoint (Shuttlegent.md section 10.1). */
namespace ShuttlegentMcpEndpointConstant
{
	// Ports probed for a free one, from FirstPort on
	inline constexpr uint32 FirstPort = 47800;
	inline constexpr uint32 PortCount = 100;

	inline const TCHAR* Path = TEXT("/mcp");
	inline const TCHAR* Host = TEXT("127.0.0.1");
	inline const TCHAR* UrlFormat = TEXT("http://{0}:{1}{2}");

	// Name of the server in the ACP mcpServers declaration; the runtime prefixes tool names with it
	inline const TCHAR* ServerName = TEXT("ue");

	// Authorization header and its scheme
	inline const TCHAR* HeaderAuthorization = TEXT("Authorization");
	inline const TCHAR* BearerPrefix = TEXT("Bearer ");
	inline const TCHAR* ContentTypeJson = TEXT("application/json");

	// MCP protocol version answered when the client names none
	inline const TCHAR* DefaultProtocolVersion = TEXT("2025-06-18");
	inline const TCHAR* ServerInfoName = TEXT("shuttlegent-editor");

	// JSON-RPC
	inline const TCHAR* JsonRpcVersion = TEXT("2.0");
	inline constexpr int32 MethodNotFound = -32601;
	inline constexpr int32 InvalidParams = -32602;
	inline constexpr int32 ParseError = -32700;

	// MCP methods
	inline const TCHAR* MethodInitialize = TEXT("initialize");
	inline const TCHAR* MethodToolsList = TEXT("tools/list");
	inline const TCHAR* MethodToolsCall = TEXT("tools/call");
	inline const TCHAR* MethodPing = TEXT("ping");

	// JSON-RPC and MCP fields
	inline const TCHAR* FieldJsonRpc = TEXT("jsonrpc");
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldMethod = TEXT("method");
	inline const TCHAR* FieldParams = TEXT("params");
	inline const TCHAR* FieldResult = TEXT("result");
	inline const TCHAR* FieldError = TEXT("error");
	inline const TCHAR* FieldCode = TEXT("code");
	inline const TCHAR* FieldMessage = TEXT("message");
	inline const TCHAR* FieldProtocolVersion = TEXT("protocolVersion");
	inline const TCHAR* FieldCapabilities = TEXT("capabilities");
	inline const TCHAR* FieldTools = TEXT("tools");
	inline const TCHAR* FieldListChanged = TEXT("listChanged");
	inline const TCHAR* FieldServerInfo = TEXT("serverInfo");
	inline const TCHAR* FieldName = TEXT("name");
	inline const TCHAR* FieldVersion = TEXT("version");
	inline const TCHAR* FieldInstructions = TEXT("instructions");
	inline const TCHAR* FieldDescription = TEXT("description");
	inline const TCHAR* FieldInputSchema = TEXT("inputSchema");
	inline const TCHAR* FieldAnnotations = TEXT("annotations");
	inline const TCHAR* FieldReadOnlyHint = TEXT("readOnlyHint");
	inline const TCHAR* FieldDestructiveHint = TEXT("destructiveHint");
	inline const TCHAR* FieldArguments = TEXT("arguments");
	inline const TCHAR* FieldContent = TEXT("content");
	inline const TCHAR* FieldType = TEXT("type");
	inline const TCHAR* FieldText = TEXT("text");
	inline const TCHAR* FieldIsError = TEXT("isError");
	inline const TCHAR* ContentTypeText = TEXT("text");

	// ACP mcpServers declaration fields
	inline const TCHAR* DeclarationTypeHttp = TEXT("http");
	inline const TCHAR* FieldUrl = TEXT("url");
	inline const TCHAR* FieldHeaders = TEXT("headers");
	inline const TCHAR* FieldValue = TEXT("value");

	// Instructions sent with initialize
	inline const TCHAR* Instructions = TEXT("Unreal Editor tools of the open project. Read tools answer at once; write tools ask the user to confirm in the editor (55 s limit) and run as one undoable transaction that is rolled back as a whole if any object fails. Object paths are full paths such as /Game/Maps/Main.Main:PersistentLevel.Cube_1. Unreal Engine toolsets (UMG, Niagara, PCG and more) are reached through list_toolsets, describe_toolset and call_toolset_tool. When a request is ambiguous, ask_user lets the user pick from your options in the editor.");
}
