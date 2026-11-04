#pragma once

#include "CoreMinimal.h"

#include "Chat/FShuttlegentQueuedPrompt.h"

class FJsonObject;
class FJsonValue;

/**
 * 草稿与排队提示的存盘（会话页签设计文档 §10.2）：每个会话一个文件 Saved/Shuttlegent/Sessions/Drafts/<SessionId>.json，
 * 新对话的草稿按视图存在 Drafts/Views/<视图键>.json。草稿是页面原样交来的对象，编辑器不解读；排队的提示由编辑器写入。
 * 读过的文件留在内存里，写入即落盘，草稿与队列都为空时删除文件。仅限游戏线程。
 */
class FShuttlegentDraftStore
{
private:
	struct FEntry
	{
		TSharedPtr<FJsonObject> Draft;
		TArray<TSharedPtr<FJsonValue>> Queue;
	};

	FString Directory;
	mutable TMap<FString, FEntry> Cache;

public:
	FShuttlegentDraftStore();

	/** 新对话所在视图的存放键；主页签为 ShuttlegentEditorDraft::HubKey，会话页签为其 ViewId。 */
	static FString MakeViewKey(const FString& ViewKey);

	/** 会话 id 或 MakeViewKey 的结果才可作为键：只含字母、数字、连字符与下划线，不能借此写到别的目录。 */
	static bool IsValidKey(const FString& Key);

	/** 存下的草稿；没有时为空。 */
	TSharedPtr<FJsonObject> GetDraft(const FString& Key) const;

	/** 页面交来的草稿 JSON；空串表示草稿已清空。返回 false 表示键或内容无效、未保存。 */
	bool SetDraftJson(const FString& Key, const FString& Json);

	/** 新对话有了会话 id：视图下的草稿归到会话名下（会话已有草稿时保留会话的），视图下的随之删除。 */
	void MoveDraft(const FString& FromKey, const FString& ToKey);

	/** 存下的排队提示，id 为 0，由调用方重新编号。 */
	TArray<FShuttlegentQueuedPrompt> GetQueue(const FString& SessionId) const;
	void SetQueue(const FString& SessionId, const TArray<FShuttlegentQueuedPrompt>& Items);

	/** 删去某个键的草稿与队列（会话被删除、会话页签被关闭）。 */
	void Remove(const FString& Key);

	/** 删去不在 KeepViewKeys（MakeViewKey 之前的视图键）中的视图草稿；主页签的始终保留。 */
	void RemoveViewsExcept(const TSet<FString>& KeepViewKeys);

private:
	const FEntry& Find(const FString& Key) const;
	void Write(const FString& Key);
	FString GetPath(const FString& Key) const;
};
