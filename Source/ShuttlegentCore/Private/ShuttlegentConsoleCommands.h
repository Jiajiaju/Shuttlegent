#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

class FJsonObject;
class FShuttlegentAgentService;

/**
 * Console commands that drive FShuttlegentAgentService without UI (Shuttlegent.*),
 * printing the conversation to the Output Log. Used for acceptance before the panel exists.
 */
class FShuttlegentConsoleCommands
{
private:
	TWeakPtr<FShuttlegentAgentService> Service;
	TArray<TUniquePtr<FAutoConsoleCommand>> Commands;
	FString MessageText;

	/** Session the commands act on: the last one created or resumed. */
	FString CurrentSessionId;
	int32 ThoughtChars = 0;

public:
	explicit FShuttlegentConsoleCommands(const TSharedRef<FShuttlegentAgentService>& InService);
	~FShuttlegentConsoleCommands();

private:
	void RegisterCommand(const TCHAR* Name, const TCHAR* Help, void (FShuttlegentConsoleCommands::*Handler)(const TArray<FString>&));

	void HandleStart(const TArray<FString>& Args);
	void HandleStop(const TArray<FString>& Args);
	void HandleNew(const TArray<FString>& Args);
	void HandleResume(const TArray<FString>& Args);
	void HandlePrompt(const TArray<FString>& Args);
	void HandleCancel(const TArray<FString>& Args);
	void HandleList(const TArray<FString>& Args);
	void HandleModel(const TArray<FString>& Args);

	void OnSessionUpdate(const FString& SessionId, const TSharedPtr<FJsonObject>& Update);
	void OnTurnFinished(const FString& SessionId, const FString& StopReason, const FString& ErrorMessage);
	void OnSessionReady(const FString& SessionId);
	void OnRuntimeExited(int32 ExitCode, bool bExpected);
	void OnError(const FString& SessionId, const FString& Message);

	void LogResult(bool bSuccess, const FString& Action, const FString& Error) const;
};
