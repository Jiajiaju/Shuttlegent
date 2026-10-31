#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "ShuttlegentSessionRecord.h"

/**
 * 位于 <Project>/Saved/Shuttlegent/Sessions 的会话索引与会话记录。
 *
 * 运行时无法回放会话（不支持 session/load），因此每次提问、session/update 与回合结束都追加到这里，
 * 重新打开会话时由界面回放。仅限游戏线程。
 */
class SHUTTLEGENTCORE_API FShuttlegentSessionStore
{
private:
	TArray<FShuttlegentSessionRecord> Records;
	FString Directory;
	bool bLoaded = false;

public:
	FShuttlegentSessionStore();

	/** 所有已知会话，最近更新的在前。 */
	const TArray<FShuttlegentSessionRecord>& GetRecords();

	const FShuttlegentSessionRecord* FindRecord(const FString& SessionId);

	/** 新建或更新一条记录并保存索引。新记录以当前时间为时间戳并排在最前；已有记录保留其 UpdatedAt 与位置。 */
	void UpsertRecord(const FShuttlegentSessionRecord& Record);

	/** 会话有了新内容（发出提问、回合结束）：UpdatedAt 设为当前时间，会话排到最前。 */
	void TouchRecord(const FString& SessionId);

	/** 从索引中移除会话并删除其记录。未知会话返回 false。 */
	bool DeleteRecord(const FString& SessionId);

	/** 设置用户指定的标题：去掉首尾空白、不能为空、最多 ShuttlegentSessionConstant::RenameMaxChars 个字符。保留 UpdatedAt。被拒绝时返回 false。 */
	bool RenameRecord(const FString& SessionId, const FString& Title);

	/** 在索引中归档或恢复会话。保留 UpdatedAt。未知会话返回 false。 */
	bool SetArchived(const FString& SessionId, bool bArchived);

	/** 追加一条记录：{"t": ISO-8601, "kind": Kind, "data": Data}。 */
	void AppendTranscript(const FString& SessionId, const FString& Kind, const TSharedPtr<FJsonObject>& Data);

	/** 读回整个会话记录，按时间先后返回各条记录对象。 */
	TArray<TSharedPtr<FJsonObject>> LoadTranscript(const FString& SessionId) const;

private:
	void EnsureLoaded();
	void SaveIndex() const;
	FString GetTranscriptPath(const FString& SessionId) const;
	FString GetIndexPath() const;
};
