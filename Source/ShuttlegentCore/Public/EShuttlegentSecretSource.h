#pragma once

#include "CoreMinimal.h"

/** Where FShuttlegentSecrets found a secret. */
enum class EShuttlegentSecretSource : uint8
{
	// Not found anywhere
	None,

	// The variable's Windows Credential Manager entry
	CredentialManager,

	// The editor process environment
	Environment,

	// The plugin's development .env file
	DevEnvFile
};
