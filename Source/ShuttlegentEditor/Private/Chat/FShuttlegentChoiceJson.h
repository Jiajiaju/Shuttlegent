#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "Chat/FShuttlegentChoiceAnswer.h"
#include "QuestionGate/FShuttlegentChoiceRequest.h"

/**
 * 选择卡与其提交内容的 JSON：会话记录（choice）与推给页面的条目字段用同一种形状，字段名见 ShuttlegentEditorWebField；
 * 页面提交的答案也按其中的 selected、notes、overall 解析。
 */
class FShuttlegentChoiceJson
{
public:
	static TSharedRef<FJsonObject> ToJson(const FShuttlegentChoiceRequest& Request, const FShuttlegentChoiceAnswer& Answer, const FString& Status);

	/** 读回 ToJson 写出的对象；没有卡片键或选项时返回 false。 */
	static bool FromJson(const FJsonObject& Object, FShuttlegentChoiceRequest& OutRequest, FShuttlegentChoiceAnswer& OutAnswer, FString& OutStatus);

	/** 页面提交的答案（JSON 对象文本）；只保留卡片上存在的选项 id，单选卡至多保留一项。 */
	static bool ParseAnswer(const FString& Json, const FShuttlegentChoiceRequest& Request, FShuttlegentChoiceAnswer& OutAnswer);
};
