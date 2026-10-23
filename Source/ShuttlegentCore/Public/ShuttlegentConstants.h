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

/** Plugin-relative and user-level paths. */
namespace ShuttlegentPath
{
	inline const TCHAR* PluginName = TEXT("Shuttlegent");

	// Runtime executable, relative to the plugin directory
	inline const TCHAR* RuntimeExecutable = TEXT("Runtime/shuttlegent-runtime.exe");

	// Shuttlegent profile patch, relative to the plugin directory
	inline const TCHAR* ProfilePatch = TEXT("Runtime/profile/shuttlegent.patch.yml");

	// Development-time secrets file, relative to the plugin directory
	inline const TCHAR* DevEnvFile = TEXT(".env");

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
