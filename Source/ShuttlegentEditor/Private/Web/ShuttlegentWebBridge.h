#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "ShuttlegentWebBridge.generated.h"

/**
 * 面板页面发送给编辑器的命令。页面调用 window.ue.bridge.<function>（名称由绑定转为小写）。
 * 仅作衔接：SShuttlegentWebPanel 以强指针保持其存活，绑定回调，并在释放前清除回调。
 */
UCLASS()
class UShuttlegentWebBridge : public UObject
{
	GENERATED_BODY()

public:
	TFunction<void()> OnReady;

	TFunction<void(const FString&, const FString&)> OnSubmit;

	TFunction<void()> OnCancel;

	TFunction<void()> OnNewConversation;

	TFunction<void()> OnReconnect;

	TFunction<void(const FString&, const FString&)> OnSetConfig;

	TFunction<void(const FString&)> OnCopyText;

	TFunction<void()> OnToggleContentBrowser;

	TFunction<void(const FString&)> OnOpenSession;

	TFunction<void(const FString&, const FString&)> OnRenameSession;

	TFunction<void(const FString&)> OnArchiveSession;

	TFunction<void(const FString&)> OnRestoreSession;

	TFunction<void(const FString&)> OnDeleteSession;

	TFunction<void(int32, int32)> OnSetPaneWidths;

	TFunction<void(int32, bool)> OnAnswerPermission;

	TFunction<void()> OnOpenSettings;

	TFunction<void(const FString&)> OnQueryReferences;

	TFunction<void(const FString&)> OnWatchReferences;

	TFunction<void(int32)> OnRemoveQueued;

	TFunction<void(int32, const FString&, const FString&)> OnTakeQueued;

	TFunction<void()> OnResumeQueue;

	TFunction<void()> OnRetry;

	TFunction<void(int32, bool)> OnAnswerWrite;

	TFunction<void(int32, const FString&, bool)> OnAnswerQuestion;

	TFunction<void(int32)> OnUndoWrite;

	TFunction<void(int32)> OnRewindTo;

	TFunction<void(int32)> OnBranchFrom;

	TFunction<void()> OnRetryStart;

	TFunction<void()> OnRetryResume;

	TFunction<void(int32)> OnAcknowledgeWrite;

	TFunction<void()> OnOpenPluginFolder;

public:
	/** 页面已完成初始化并请求完整状态；每次文档加载调用一次。 */
	UFUNCTION()
	void Ready();

	/** 将草稿连同其引用标签（JSON 数组）作为提示发送；结果通过 onSubmitResult 返回。 */
	UFUNCTION()
	void Submit(const FString& Text, const FString& ReferencesJson);

	/** 停止正在运行的回合。 */
	UFUNCTION()
	void Cancel();

	UFUNCTION()
	void NewConversation();

	/** 重启已丢失的运行时并恢复上一个会话。 */
	UFUNCTION()
	void Reconnect();

	/** 修改会话配置选项（"model"、"reasoning_effort"）。 */
	UFUNCTION()
	void SetConfig(const FString& ConfigId, const FString& Value);

	/** 将文本放入系统剪贴板（file:// 页面无法使用页面的剪贴板 API）。 */
	UFUNCTION()
	void CopyText(const FString& Text);

	/** 页面的添加引用按钮：打开或关闭 Content Browser 抽屉。 */
	UFUNCTION()
	void ToggleContentBrowser();

	/** 显示已存储会话的对话记录，并在运行时中恢复该会话。 */
	UFUNCTION()
	void OpenSession(const FString& SessionId);

	/** 重命名已存储的会话；编辑器会裁剪名称首尾空白，并拒绝空名称或过长的名称。 */
	UFUNCTION()
	void RenameSession(const FString& SessionId, const FString& Title);

	/** 从列表中隐藏会话；正在运行或等待权限的会话会被拒绝。 */
	UFUNCTION()
	void ArchiveSession(const FString& SessionId);

	UFUNCTION()
	void RestoreSession(const FString& SessionId);

	/** 永久删除会话的记录与对话记录；正在运行或等待权限的会话会被拒绝。 */
	UFUNCTION()
	void DeleteSession(const FString& SessionId);

	/** 停靠窗格的尺寸已调整：以 CSS 像素记录侧栏与检视器的宽度，供下次页面加载使用。 */
	UFUNCTION()
	void SetPaneWidths(int32 SidebarWidth, int32 InspectorWidth);

	/** 答复决策卡片所显示的运行时权限请求。 */
	UFUNCTION()
	void AnswerPermission(int32 EntryId, bool bAllow);

	/** 打开 Editor Preferences 中的 Shuttlegent 页面。 */
	UFUNCTION()
	void OpenSettings();

	/** @ 菜单的查询已变化；候选项通过 onReferenceResults 返回。 */
	UFUNCTION()
	void QueryReferences(const FString& Query);

	/** 草稿的引用标签（JSON 数组）已变化；其存在状态立即并在每次变化时通过 onReferenceValidity 返回。 */
	UFUNCTION()
	void WatchReferences(const FString& ReferencesJson);

	/** 移除一条排队的提示；不影响正在运行的回合。 */
	UFUNCTION()
	void RemoveQueued(int32 ItemId);

	/** 将一条排队的提示取回草稿；当前草稿（文本与引用的 JSON 数组）占据其位置。该条目通过 onQueueItemTaken 返回。 */
	UFUNCTION()
	void TakeQueued(int32 ItemId, const FString& DraftText, const FString& DraftReferencesJson);

	/** 继续已暂停的队列。 */
	UFUNCTION()
	void ResumeQueue();

	/** 重新发送失败回合的提示。 */
	UFUNCTION()
	void Retry();

	/** 确认或拒绝确认卡片所显示的编辑器写入。 */
	UFUNCTION()
	void AnswerWrite(int32 RequestId, bool bApprove);

	/** 以所选选项或用户自行输入的文本答复 Agent 提出的问题（ask_user），或跳过该问题。 */
	UFUNCTION()
	void AnswerQuestion(int32 QuestionId, const FString& Answer, bool bSkip);

	/** 写入结果上的“撤销本次修改”。 */
	UFUNCTION()
	void UndoWrite(int32 EntryId);

	/** 用户消息上的“回到这里”（页面已让用户确认）。 */
	UFUNCTION()
	void RewindTo(int32 EntryId);

	/** 用户消息上的“分支”。 */
	UFUNCTION()
	void BranchFrom(int32 EntryId);

	/** 启动引导卡片上的“重试”：再次启动运行时（若正在运行则重启）。 */
	UFUNCTION()
	void RetryStart();

	/** 恢复失败横幅上的“重试恢复”。 */
	UFUNCTION()
	void RetryResume();

	/** 结果未知的写入上的“已核对”。 */
	UFUNCTION()
	void AcknowledgeWrite(int32 EntryId);

	/** 启动引导卡片上的“打开插件目录”，Runtime/ 位于该目录下。 */
	UFUNCTION()
	void OpenPluginFolder();
};
