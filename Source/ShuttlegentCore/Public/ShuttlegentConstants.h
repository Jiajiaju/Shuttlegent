#pragma once

#include "CoreMinimal.h"

/** ACP method names (Agent Client Protocol v1). */
namespace ShuttlegentAcpMethod
{
	inline const TCHAR* Initialize = TEXT("initialize");
	inline const TCHAR* SessionNew = TEXT("session/new");
	inline const TCHAR* SessionResume = TEXT("session/resume");
	inline const TCHAR* SessionClose = TEXT("session/close");
	inline const TCHAR* SessionPrompt = TEXT("session/prompt");
	inline const TCHAR* SessionCancel = TEXT("session/cancel");
	inline const TCHAR* SessionUpdate = TEXT("session/update");
	inline const TCHAR* SessionSetConfigOption = TEXT("session/set_config_option");
	inline const TCHAR* SessionRequestPermission = TEXT("session/request_permission");
}

/** ACP protocol values. */
namespace ShuttlegentAcpConstant
{
	// ACP protocol version this client speaks
	inline constexpr int32 ProtocolVersion = 1;

	// JSON-RPC "method not found" error code
	inline constexpr int32 MethodNotFoundErrorCode = -32601;

	// JSON-RPC error code used locally when the runtime exits before answering
	inline constexpr int32 RuntimeExitedErrorCode = -32000;

	// Permission option kinds offered by session/request_permission
	inline const TCHAR* PermissionKindAllowOnce = TEXT("allow_once");
	inline const TCHAR* PermissionKindRejectOnce = TEXT("reject_once");
}

/** Environment variables passed to the DeepSeek Harness runtime. */
namespace ShuttlegentEnvVar
{
	inline const TCHAR* DshHome = TEXT("DSH_HOME");
	inline const TCHAR* DshAgentsHome = TEXT("DSH_AGENTS_HOME");
	inline const TCHAR* DshPermissionMode = TEXT("DSH_PERMISSION_MODE");
	inline const TCHAR* DshTelemetryDisabled = TEXT("DSH_TELEMETRY_DISABLED");
	inline const TCHAR* LocalAppData = TEXT("LOCALAPPDATA");

	// Variable the built-in DeepSeek route (deepseek-official) reads its key from; filled from UShuttlegentSettings::DeepSeekApiKey
	inline const TCHAR* ProviderKey = TEXT("DEEPSEEK_API_KEY");

	// Display name of that provider in the start guide card
	inline const TCHAR* ProviderName = TEXT("DeepSeek");

	// Value written to DSH_TELEMETRY_DISABLED; any non-empty value opts out
	inline const TCHAR* TelemetryDisabledValue = TEXT("1");

	// Variables copied from the editor environment; everything else is dropped
	inline const TCHAR* InheritedNames[] =
	{
		TEXT("SystemRoot"),
		TEXT("SystemDrive"),
		TEXT("windir"),
		TEXT("PATH"),
		TEXT("PATHEXT"),
		TEXT("COMSPEC"),
		TEXT("TEMP"),
		TEXT("TMP"),
		TEXT("USERPROFILE"),
		TEXT("APPDATA"),
		TEXT("LOCALAPPDATA"),
		TEXT("HOMEDRIVE"),
		TEXT("HOMEPATH"),
		TEXT("USERNAME"),
		TEXT("COMPUTERNAME"),
	};
}

/** Model providers and the generated runtime patch (Shuttlegent.md sections 8.2, 9). */
namespace ShuttlegentProviderConstant
{
	// The runtime's built-in DeepSeek route, used when the project names no default provider
	inline const TCHAR* BuiltinRoute = TEXT("deepseek-official");

	// Official endpoint of the built-in route (Anthropic Messages); default of UShuttlegentSettings::DeepSeekBaseUrl
	inline const TCHAR* DeepSeekBaseUrl = TEXT("https://api.deepseek.com/anthropic");

	// DeepSeek's OpenAI-compatible endpoint; default base URL of a new provider entry
	inline const TCHAR* DeepSeekOpenAiBaseUrl = TEXT("https://api.deepseek.com");

	// Key variable of a provider entry that names none: SHUTTLEGENT_<ROUTE>_API_KEY, dashes as underscores
	inline const TCHAR* KeyVariablePrefix = TEXT("SHUTTLEGENT_");
	inline const TCHAR* KeyVariableSuffix = TEXT("_API_KEY");
	inline const TCHAR* RouteSeparator = TEXT("-");
	inline const TCHAR* VariableSeparator = TEXT("_");

	// llm-pi-ai api values (EShuttlegentProviderApi)
	inline const TCHAR* ApiOpenAiCompletions = TEXT("openai-completions");
	inline const TCHAR* ApiOpenAiResponses = TEXT("openai-responses");
	inline const TCHAR* ApiAnthropicMessages = TEXT("anthropic-messages");

	// Patch the plugin writes into DSH_HOME before every launch and passes as a second --patch (JSON is valid YAML)
	inline const TCHAR* GeneratedPatchFile = TEXT("generated.patch.yml");

	// Rows and fields of the generated patch
	inline const TCHAR* RowLlmPiAi = TEXT("llm-pi-ai");
	inline const TCHAR* RowLlmDeepSeek = TEXT("llm-deepseek");
	inline const TCHAR* RowAcp = TEXT("acp");
	inline const TCHAR* RowSkillFilesystem = TEXT("skill-filesystem");
	inline const TCHAR* RowToolPwsh = TEXT("tool-pwsh");
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldConfig = TEXT("config");
	inline const TCHAR* FieldDisabled = TEXT("disabled");
	inline const TCHAR* FieldProviders = TEXT("providers");
	inline const TCHAR* FieldDisplayName = TEXT("displayName");
	inline const TCHAR* FieldApiKeyEnv = TEXT("apiKeyEnv");
	inline const TCHAR* FieldApi = TEXT("api");
	inline const TCHAR* FieldBaseUrl = TEXT("baseURL");
	inline const TCHAR* FieldModels = TEXT("models");
	inline const TCHAR* FieldModelId = TEXT("id");
	inline const TCHAR* FieldModelName = TEXT("name");
	inline const TCHAR* FieldProvider = TEXT("provider");
	inline const TCHAR* FieldModel = TEXT("model");
	inline const TCHAR* FieldIncludeDefaultRoots = TEXT("includeDefaultRoots");
	inline const TCHAR* FieldCustomSkillDirs = TEXT("customSkillDirs");
}

/** Plugin-relative and user-level paths. */
namespace ShuttlegentPath
{
	inline const TCHAR* PluginName = TEXT("Shuttlegent");

	// Runtime executable, relative to the plugin directory
	inline const TCHAR* RuntimeExecutable = TEXT("Runtime/shuttlegent-runtime.exe");

	// Shuttlegent profile patch, relative to the plugin directory
	inline const TCHAR* ProfilePatch = TEXT("Runtime/profile/shuttlegent.patch.yml");

	// dsh profile launched over ACP
	inline const TCHAR* AcpProfile = TEXT("acp");

	// DSH_HOME, relative to %LOCALAPPDATA%; also the runtime's launch directory, which must be on the system drive
	inline const TCHAR* DshHome = TEXT("Shuttlegent/dsh-home");

	// DSH_AGENTS_HOME, relative to DSH_HOME
	inline const TCHAR* DshAgentsHome = TEXT("agents");

	// Session transcripts and index, relative to the project Saved directory
	inline const TCHAR* SessionsDir = TEXT("Shuttlegent/Sessions");
	inline const TCHAR* SessionIndexFile = TEXT("index.json");
	inline const TCHAR* TranscriptExtension = TEXT(".jsonl");
}

/** Runtime process timings. */
namespace ShuttlegentProcessConstant
{
	// Time the runtime gets to exit after stdin closes before it is terminated, in seconds
	inline constexpr float GracefulExitTimeoutSeconds = 5.0f;

	// Size of each pipe read, in bytes
	inline constexpr int32 PipeReadChunkBytes = 64 * 1024;

	// Exit code used when the runtime is terminated by Shuttlegent
	inline constexpr int32 TerminatedExitCode = -1;
}

/** Session bookkeeping. */
namespace ShuttlegentSessionConstant
{
	// Maximum length of a session title derived from the first prompt, in characters
	inline constexpr int32 TitleMaxChars = 40;

	// Maximum length of a title the user sets by renaming, in characters (Shuttlegent UI interaction spec section 6)
	inline constexpr int32 RenameMaxChars = 60;
}

namespace ShuttlegentAppearanceConstant
{
	// Conversation font size of the panel page, in CSS pixels (UShuttlegentSettings::FontSize)
	inline constexpr int32 DefaultFontSize = 13;
}
