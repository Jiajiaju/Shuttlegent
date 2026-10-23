#pragma once

#include "CoreMinimal.h"

/** Everything needed to start the DeepSeek Harness runtime process. */
struct FShuttlegentRuntimeLaunchSpec
{
	/** Absolute path of the runtime executable. */
	FString Executable;

	/** Command-line arguments, unquoted. */
	TArray<FString> Arguments;

	/** Launch directory of the process (DSH_HOME, on the system drive). */
	FString WorkingDirectory;

	/** Complete environment of the process; nothing else is inherited. */
	TMap<FString, FString> Environment;

	/**
	 * Builds the launch spec from the plugin layout, user settings and the development .env file.
	 * @param OutSpec   Filled on success.
	 * @param OutError  Human-readable reason on failure.
	 * @return true when the runtime executable and profile patch exist and DSH_HOME could be created.
	 */
	static bool Build(FShuttlegentRuntimeLaunchSpec& OutSpec, FString& OutError);
};
