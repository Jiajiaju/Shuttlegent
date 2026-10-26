#include "FShuttlegentSecrets.h"

#include "HAL/PlatformMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentSettings.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincred.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif // PLATFORM_WINDOWS

namespace FShuttlegentSecretsPrivate
{
	// .env syntax
	inline const TCHAR* CommentPrefix = TEXT("#");
	inline const TCHAR* Assignment = TEXT("=");
	inline const TCHAR* DoubleQuote = TEXT("\"");
	inline const TCHAR* SingleQuote = TEXT("'");

	FString StripQuotes(const FString& Value)
	{
		const bool bDoubleQuoted = Value.Len() >= 2 && Value.StartsWith(DoubleQuote) && Value.EndsWith(DoubleQuote);
		const bool bSingleQuoted = Value.Len() >= 2 && Value.StartsWith(SingleQuote) && Value.EndsWith(SingleQuote);
		if (bDoubleQuoted || bSingleQuoted) return Value.Mid(1, Value.Len() - 2);
		return Value;
	}

	/** The value of Variable in the development .env file (KEY=VALUE lines); false when it has none. */
	bool ReadDevEnvFile(const FString& Variable, FString& OutValue)
	{
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *FShuttlegentSecrets::GetDevEnvFilePath())) return false;

		for (const FString& RawLine : Lines)
		{
			const FString Line = RawLine.TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(CommentPrefix)) continue;

			FString Name;
			FString Value;
			if (!Line.Split(Assignment, &Name, &Value) || Name.TrimStartAndEnd() != Variable) continue;

			OutValue = StripQuotes(Value.TrimStartAndEnd());
			return !OutValue.IsEmpty();
		}
		return false;
	}

	/** A generic Windows credential's password, as cmdkey /generic stores it (UTF-16). */
	bool ReadCredential(const FString& Target, FString& OutValue)
	{
#if PLATFORM_WINDOWS
		PCREDENTIALW Credential = nullptr;
		if (!CredReadW(*Target, CRED_TYPE_GENERIC, 0, &Credential) || Credential == nullptr) return false;

		const int32 Chars = static_cast<int32>(Credential->CredentialBlobSize / sizeof(WIDECHAR));
		OutValue = FString::ConstructFromPtrSize(reinterpret_cast<const WIDECHAR*>(Credential->CredentialBlob), Chars);
		CredFree(Credential);
		return !OutValue.IsEmpty();
#else
		return false;
#endif // PLATFORM_WINDOWS
	}
}

bool FShuttlegentSecrets::Resolve(const FString& Variable, FString& OutValue, EShuttlegentSecretSource& OutSource)
{
	OutValue.Reset();
	OutSource = EShuttlegentSecretSource::None;
	if (Variable.IsEmpty()) return false;

	if (FShuttlegentSecretsPrivate::ReadCredential(GetCredentialTarget(Variable), OutValue))
	{
		OutSource = EShuttlegentSecretSource::CredentialManager;
		return true;
	}

	OutValue = FPlatformMisc::GetEnvironmentVariable(*Variable);
	if (!OutValue.IsEmpty())
	{
		OutSource = EShuttlegentSecretSource::Environment;
		return true;
	}

	if (FShuttlegentSecretsPrivate::ReadDevEnvFile(Variable, OutValue))
	{
		OutSource = EShuttlegentSecretSource::DevEnvFile;
		return true;
	}
	return false;
}

FString FShuttlegentSecrets::GetCredentialTarget(const FString& Variable)
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	const FString* Configured = IsValid(Settings) ? Settings->CredentialTargets.Find(Variable) : nullptr;
	if (Configured != nullptr && !Configured->IsEmpty()) return *Configured;

	return FString(ShuttlegentProviderConstant::CredentialPrefix) + Variable;
}

FString FShuttlegentSecrets::GetDevEnvFilePath()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
	const FString PluginDir = Plugin.IsValid() ? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()) : FString();
	return FPaths::Combine(PluginDir, ShuttlegentPath::DevEnvFile);
}
