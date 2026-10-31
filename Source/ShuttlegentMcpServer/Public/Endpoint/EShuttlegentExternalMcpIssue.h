#pragma once

#include "CoreMinimal.h"

/** 项目设置中的外部 MCP 服务器未被纳入运行时 mcpServers 声明的原因。 */
enum class EShuttlegentExternalMcpIssue : uint8
{
	// 随每个会话一同声明
	None,

	// 名称为空，或已被编辑器端点占用
	NameInvalid,

	// 另一个外部服务器使用了相同的名称
	NameTaken,

	// Stdio：命令不是绝对路径
	CommandNotAbsolute,

	// Http：缺少 URL
	UrlMissing,

	// Http：某个请求头引用的变量没有值
	HeaderValueMissing
};
