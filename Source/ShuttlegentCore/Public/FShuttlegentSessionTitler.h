#pragma once

#include "CoreMinimal.h"

/**
 * 会话的概括标题：第一轮回答结束后，编辑器直接向会话所用的模型发一次小请求，
 * 要一个不超过 16 个字的标题。地址、协议、请求头与密钥的取法与检测连接（FShuttlegentModelProbe）一致；
 * 日志只记录地址、状态码与耗时，不记录密钥、消息与标题内容。仅限游戏线程。
 */
struct SHUTTLEGENTCORE_API FShuttlegentSessionTitler
{
	DECLARE_DELEGATE_OneParam(FOnTitled, const FString& /*Title*/);

	/**
	 * ModelValue 为会话 "model" 配置项的取值（["路由", "模型 ID"] 的 JSON 文本），对不上任何模型时用默认模型。
	 * OnDone 在游戏线程上恰好调用一次；请求失败、缺少地址或密钥、返回为空时以空字符串调用。
	 */
	static void Generate(const FString& ModelValue, const FString& UserText, const FString& AnswerText, FOnTitled OnDone);
};
