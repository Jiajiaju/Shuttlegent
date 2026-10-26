#include "ShuttlegentConsoleCommands.h"

#include "Dom/JsonObject.h"

#include "ShuttlegentAgentService.h"
#include "ShuttlegentLog.h"

namespace ShuttlegentConsoleCommandsPrivate
{
	inline const TCHAR* CommandStart = TEXT("Shuttlegent.Start");
	inline const TCHAR* CommandStop = TEXT("Shuttlegent.Stop");
	inline const TCHAR* CommandNew = TEXT("Shuttlegent.New");
	inline const TCHAR* CommandResume = TEXT("Shuttlegent.Resume");
	inline const TCHAR* CommandPrompt = TEXT("Shuttlegent.Prompt");
	inline const TCHAR* CommandCancel = TEXT("Shuttlegent.Cancel");
	inline const TCHAR* CommandList = TEXT("Shuttlegent.List");
	inline const TCHAR* CommandModel = TEXT("Shuttlegent.Model");

	// session/update fields printed to the log
	inline const TCHAR* FieldSessionUpdate = TEXT("sessionUpdate");
	inline const TCHAR* FieldContent = TEXT("content");
	inline const TCHAR* FieldText = TEXT("text");
	inline const TCHAR* FieldTitle = TEXT("title");
	inline const TCHAR* FieldStatus = TEXT("status");
	inline const TCHAR* FieldToolCallId = TEXT("toolCallId");

	inline const TCHAR* KindMessageChunk = TEXT("agent_message_chunk");
	inline const TCHAR* KindThoughtChunk = TEXT("agent_thought_chunk");
	inline const TCHAR* KindToolCall = TEXT("tool_call");
	inline const TCHAR* KindToolCallUpdate = TEXT("tool_call_update");

	// Config option changed by Shuttlegent.Model
	inline const TCHAR* ConfigIdModel = TEXT("model");

	FString ChunkText(const TSharedPtr<FJsonObject>& Update)
	{
		const TSharedPtr<FJsonObject>* Content = nullptr;
		FString Text;
		if (Update->TryGetObjectField(FieldContent, Content)) (*Content)->TryGetStringField(FieldText, Text);
		return Text;
	}
}

FShuttlegentConsoleCommands::FShuttlegentConsoleCommands(const TSharedRef<FShuttlegentAgentService>& InService)
	: Service(InService)
{
	RegisterCommand(ShuttlegentConsoleCommandsPrivate::CommandStart, TEXT("Launch the Shuttlegent agent runtime."), &FShuttlegentConsoleCommands::HandleStart);
	RegisterCommand(ShuttlegentConsoleCommandsPrivate::CommandStop, TEXT("Shut the Shuttlegent agent runtime down."), &FShuttlegentConsoleCommands::HandleStop);
	RegisterCommand(ShuttlegentConsoleCommandsPrivate::CommandNew, TEXT("Create a new agent session in the project directory."), &FShuttlegentConsoleCommands::HandleNew);
	RegisterCommand(ShuttlegentConsoleCommandsPrivate::CommandResume, TEXT("Resume a session: Shuttlegent.Resume <sessionId>."), &FShuttlegentConsoleCommands::HandleResume);
	RegisterCommand(ShuttlegentConsoleCommandsPrivate::CommandPrompt, TEXT("Send a prompt: Shuttlegent.Prompt <text>."), &FShuttlegentConsoleCommands::HandlePrompt);
	RegisterCommand(ShuttlegentConsoleCommandsPrivate::CommandCancel, TEXT("Cancel the running prompt."), &FShuttlegentConsoleCommands::HandleCancel);
	RegisterCommand(ShuttlegentConsoleCommandsPrivate::CommandList, TEXT("List known sessions, most recent first."), &FShuttlegentConsoleCommands::HandleList);
	RegisterCommand(ShuttlegentConsoleCommandsPrivate::CommandModel, TEXT("Switch model: Shuttlegent.Model <value from the model config option>."), &FShuttlegentConsoleCommands::HandleModel);

	InService->OnSessionUpdate.AddRaw(this, &FShuttlegentConsoleCommands::OnSessionUpdate);
	InService->OnTurnFinished.AddRaw(this, &FShuttlegentConsoleCommands::OnTurnFinished);
	InService->OnSessionReady.AddRaw(this, &FShuttlegentConsoleCommands::OnSessionReady);
	InService->OnRuntimeExited.AddRaw(this, &FShuttlegentConsoleCommands::OnRuntimeExited);
	InService->OnError.AddRaw(this, &FShuttlegentConsoleCommands::OnError);
}

FShuttlegentConsoleCommands::~FShuttlegentConsoleCommands()
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	Pinned->OnSessionUpdate.RemoveAll(this);
	Pinned->OnTurnFinished.RemoveAll(this);
	Pinned->OnSessionReady.RemoveAll(this);
	Pinned->OnRuntimeExited.RemoveAll(this);
	Pinned->OnError.RemoveAll(this);
}

void FShuttlegentConsoleCommands::RegisterCommand(const TCHAR* Name, const TCHAR* Help, void (FShuttlegentConsoleCommands::*Handler)(const TArray<FString>&))
{
	Commands.Add(MakeUnique<FAutoConsoleCommand>(Name, Help, FConsoleCommandWithArgsDelegate::CreateRaw(this, Handler)));
}

void FShuttlegentConsoleCommands::HandleStart(const TArray<FString>& Args)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	FString Error;
	LogResult(Pinned->Start(Error), TEXT("Start"), Error);
}

void FShuttlegentConsoleCommands::HandleStop(const TArray<FString>& Args)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid()) Pinned->Stop();
}

void FShuttlegentConsoleCommands::HandleNew(const TArray<FString>& Args)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	FString Error;
	LogResult(Pinned->NewSession(Error), TEXT("New session"), Error);
}

void FShuttlegentConsoleCommands::HandleResume(const TArray<FString>& Args)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	FString Error;
	const FString SessionId = Args.Num() > 0 ? Args[0] : FString();
	LogResult(Pinned->ResumeSession(SessionId, Error), TEXT("Resume session"), Error);
}

void FShuttlegentConsoleCommands::HandlePrompt(const TArray<FString>& Args)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	MessageText.Reset();
	ThoughtChars = 0;

	FString Error;
	const FString Text = FString::Join(Args, TEXT(" "));
	LogResult(Pinned->Prompt(CurrentSessionId, Text, Error), TEXT("Prompt"), Error);
}

void FShuttlegentConsoleCommands::HandleCancel(const TArray<FString>& Args)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid()) Pinned->Cancel(CurrentSessionId);
}

void FShuttlegentConsoleCommands::HandleList(const TArray<FString>& Args)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	const TArray<FShuttlegentSessionRecord>& Records = Pinned->GetSessionStore().GetRecords();
	UE_LOG(LogShuttlegent, Display, TEXT("%d session(s):"), Records.Num());
	for (const FShuttlegentSessionRecord& Record : Records)
	{
		UE_LOG(LogShuttlegent, Display, TEXT("  %s  %s  [%s]  %s"), *Record.SessionId, *Record.UpdatedAt.ToString(), *Record.Model, *Record.Title);
	}
}

void FShuttlegentConsoleCommands::HandleModel(const TArray<FString>& Args)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	FString Error;
	const FString Value = FString::Join(Args, TEXT(" "));
	LogResult(Pinned->SetConfigOption(CurrentSessionId, ShuttlegentConsoleCommandsPrivate::ConfigIdModel, Value, Error), TEXT("Set model"), Error);
}

void FShuttlegentConsoleCommands::OnSessionUpdate(const FString& SessionId, const TSharedPtr<FJsonObject>& Update)
{
	FString Kind;
	Update->TryGetStringField(ShuttlegentConsoleCommandsPrivate::FieldSessionUpdate, Kind);

	if (Kind == ShuttlegentConsoleCommandsPrivate::KindMessageChunk)
	{
		MessageText.Append(ShuttlegentConsoleCommandsPrivate::ChunkText(Update));
		return;
	}
	if (Kind == ShuttlegentConsoleCommandsPrivate::KindThoughtChunk)
	{
		ThoughtChars += ShuttlegentConsoleCommandsPrivate::ChunkText(Update).Len();
		return;
	}
	if (Kind == ShuttlegentConsoleCommandsPrivate::KindToolCall || Kind == ShuttlegentConsoleCommandsPrivate::KindToolCallUpdate)
	{
		FString Title;
		FString Status;
		FString ToolCallId;
		Update->TryGetStringField(ShuttlegentConsoleCommandsPrivate::FieldTitle, Title);
		Update->TryGetStringField(ShuttlegentConsoleCommandsPrivate::FieldStatus, Status);
		Update->TryGetStringField(ShuttlegentConsoleCommandsPrivate::FieldToolCallId, ToolCallId);
		UE_LOG(LogShuttlegent, Display, TEXT("[tool] %s %s %s"), *ToolCallId, *Title, *Status);
		return;
	}

	UE_LOG(LogShuttlegent, Verbose, TEXT("[update] %s"), *Kind);
}

void FShuttlegentConsoleCommands::OnTurnFinished(const FString& SessionId, const FString& StopReason, const FString& ErrorMessage)
{
	if (ThoughtChars > 0) UE_LOG(LogShuttlegent, Display, TEXT("[thought] %d characters"), ThoughtChars);
	if (!MessageText.IsEmpty()) UE_LOG(LogShuttlegent, Display, TEXT("[assistant]\n%s"), *MessageText);

	if (ErrorMessage.IsEmpty())
	{
		UE_LOG(LogShuttlegent, Display, TEXT("Turn finished: %s"), *StopReason);
	}
	else
	{
		UE_LOG(LogShuttlegent, Error, TEXT("Turn failed: %s"), *ErrorMessage);
	}

	MessageText.Reset();
	ThoughtChars = 0;
}

void FShuttlegentConsoleCommands::OnSessionReady(const FString& SessionId)
{
	CurrentSessionId = SessionId;
	UE_LOG(LogShuttlegent, Display, TEXT("Session ready: %s"), *SessionId);
}

void FShuttlegentConsoleCommands::OnRuntimeExited(int32 ExitCode, bool bExpected)
{
	UE_LOG(LogShuttlegent, Display, TEXT("Runtime exited (code %d, %s)."), ExitCode, bExpected ? TEXT("requested") : TEXT("unexpected"));
}

void FShuttlegentConsoleCommands::OnError(const FString& SessionId, const FString& Message)
{
	UE_LOG(LogShuttlegent, Error, TEXT("%s"), *Message);
}

void FShuttlegentConsoleCommands::LogResult(bool bSuccess, const FString& Action, const FString& Error) const
{
	if (bSuccess)
	{
		UE_LOG(LogShuttlegent, Display, TEXT("%s requested."), *Action);
	}
	else
	{
		UE_LOG(LogShuttlegent, Error, TEXT("%s failed: %s"), *Action, *Error);
	}
}
