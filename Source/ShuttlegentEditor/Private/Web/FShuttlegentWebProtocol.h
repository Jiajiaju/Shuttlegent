#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class FJsonObject;
class FShuttlegentChatModel;
class UObject;
struct FPropertyChangedEvent;
struct FShuttlegentChatEntry;

/**
 * 编辑器推送给面板页面的内容：页面报告就绪时推送完整快照，之后推送增量。
 *
 * 监听 FShuttlegentChatModel，仅标记发生变化的部分；由核心 ticker 将一帧内的全部变化
 * 合并为一条 JSON 消息，因此流式分块不会各自产生一次页面调用。条目仅在其指纹自上一条
 * 消息以来发生变化时才重新发送。仅限游戏线程。
 */
class FShuttlegentWebProtocol : public TSharedFromThis<FShuttlegentWebProtocol>
{
private:
	TSharedPtr<FShuttlegentChatModel> Model;

	/** 向页面投递一条 JSON 消息。 */
	TFunction<void(const FString&)> Push;

	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle SettingsChangedHandle;
	FDelegateHandle ProjectSettingsChangedHandle;

	/** 各条目最近一次发送时的指纹，以及最近一次发送时的条目顺序。 */
	TMap<int32, uint32> SentFingerprints;
	TArray<int32> SentOrder;

	/** 等待随下一条消息发送的一次性事件。 */
	TArray<FString> PendingEvents;

	bool bPageReady = false;
	bool bResetPending = false;
	bool bStatusDirty = false;
	bool bEntriesDirty = false;
	bool bSessionsDirty = false;
	bool bRunLogDirty = false;
	bool bQueueDirty = false;
	bool bMcpDirty = false;

public:
	void Initialize(const TSharedRef<FShuttlegentChatModel>& InModel, TFunction<void(const FString&)> InPush);
	void Shutdown();

	/** 页面已（重新）加载并请求完整状态；在此之前不推送任何内容。 */
	void HandlePageReady();

private:
	bool HandleTick(float DeltaTime);
	void Flush();

	void WriteStatus(const TSharedRef<FJsonObject>& Root) const;
	void WriteConfig(const TSharedRef<FJsonObject>& Root) const;

	/** 斜杠模板，每次快照时从模板目录读取（因此页面重新加载即可获得修改）。 */
	void WriteTemplates(const TSharedRef<FJsonObject>& Root) const;

	/** 用户拖拽调整的窗格宽度，取自按用户保存的配置；仅随快照发送。 */
	void WritePaneWidths(const TSharedRef<FJsonObject>& Root) const;

	/** 活动与已归档的会话、当前会话及其标题、重命名长度上限与权限预设。 */
	void WriteSessions(const TSharedRef<FJsonObject>& Root) const;
	void WriteRunLog(const TSharedRef<FJsonObject>& Root) const;

	/** 当前会话的队列，以及失败的回合能否重试。 */
	void WriteQueue(const TSharedRef<FJsonObject>& Root) const;

	/** MCP 卡片：编辑器端点及其工具、引擎 Toolset、项目的外部服务器。 */
	void WriteMcp(const TSharedRef<FJsonObject>& Root) const;

	/** 最早一个未答复的运行时权限请求及其所涉及的内容，或 null。 */
	void WritePendingPermission(const TSharedRef<FJsonObject>& Root) const;

	/** 最早一个等待确认的编辑器写入，含其对象表与原始参数，或 null。 */
	void WritePendingWrite(const TSharedRef<FJsonObject>& Root) const;
	void WritePendingQuestion(const TSharedRef<FJsonObject>& Root) const;

	/** 添加指纹发生变化的条目（重置时为全部条目），以及发生变化时的顺序。 */
	void WriteEntries(const TSharedRef<FJsonObject>& Root, bool bAll);

	TSharedRef<FJsonObject> MakeEntry(const FShuttlegentChatEntry& Entry) const;

	void HandleStatusChanged();
	void HandleEntriesChanged();
	void HandleSessionsChanged();
	void HandleRunLogChanged();
	void HandleQueueChanged();
	void HandleTurnFinished(bool bCompleted);
	void HandleSettingChanged(UObject* Settings, FPropertyChangedEvent& Event);
	void HandleProjectSettingChanged(UObject* Settings, FPropertyChangedEvent& Event);
};
