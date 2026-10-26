#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "Tool/FShuttlegentToolDefinition.h"
#include "Tool/FShuttlegentToolResult.h"
#include "WriteGate/FShuttlegentWriteObject.h"

struct FShuttlegentWriteRequest;

/**
 * One editor tool the MCP endpoint offers. Runs on the game thread.
 *
 * A read-only tool implements Run. A write tool implements Preview and Execute instead: Preview describes what would
 * change (the confirmation card), the endpoint asks the write gate, and only after confirmation Execute writes the
 * previewed objects. A transactional write runs inside one editor transaction that the endpoint opens; Execute must
 * call Modify() on what it changes and must not open its own transaction.
 */
class SHUTTLEGENTMCPSERVER_API IShuttlegentTool
{
public:
	virtual ~IShuttlegentTool() = default;

	virtual const FShuttlegentToolDefinition& GetDefinition() const = 0;

	/** Read-only tools: answers the call. */
	virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments);

	/**
	 * Write tools: fills OutRequest's Action, Level and Objects (name, path, field, current and planned value) without
	 * changing anything. Returns false with OutError to refuse the call before anyone is asked.
	 */
	virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError);

	/** Write tools, after confirmation: writes the previewed objects in order, setting each one's Status and, unless completed, Message. */
	virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects);
};
