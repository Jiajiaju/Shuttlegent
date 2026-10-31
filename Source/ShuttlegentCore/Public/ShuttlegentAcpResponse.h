#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** 发给 Agent 运行时的一个 JSON-RPC 请求的结果。 */
struct FShuttlegentAcpResponse
{
	/** "result" 答复时为 true，"error" 答复或本地失败时为 false。 */
	bool bSuccess = false;

	/** "result" 对象；不含字段的结果可能为空。 */
	TSharedPtr<FJsonObject> Result;

	/** bSuccess 为 false 时的 JSON-RPC 错误码。 */
	int32 ErrorCode = 0;

	/** bSuccess 为 false 时的 JSON-RPC 错误消息。 */
	FString ErrorMessage;
};
