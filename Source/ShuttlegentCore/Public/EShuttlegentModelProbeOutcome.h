#pragma once

#include "CoreMinimal.h"

/** 一次检测连接的结论。 */
enum class EShuttlegentModelProbeOutcome : uint8
{
	// 2xx：地址、协议、密钥与模型 ID 都正确
	Connected,

	// 429：已连通，当前被限流
	RateLimited,

	// 401 / 403：密钥无效或无权限
	KeyInvalid,

	// 404 或返回 HTML：地址与协议不配套
	ProtocolMismatch,

	// 400 且错误内容涉及模型：模型 ID 不对
	WrongModel,

	// 其他 4xx / 5xx
	HttpError,

	// DNS 失败、拒绝连接或超时
	Unreachable,

	// 设置中没有密钥，未发请求
	MissingKey,

	// 设置中没有地址，未发请求
	MissingBaseUrl,

	// 条目没有模型 ID，未发请求
	MissingModel
};
