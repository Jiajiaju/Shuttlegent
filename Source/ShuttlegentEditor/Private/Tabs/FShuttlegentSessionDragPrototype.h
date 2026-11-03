#pragma once

#include "CoreMinimal.h"

class SDockTab;

/**
 * 会话页签方案 M0 的拖拽原型（工作区 Doc/ShuttlegentSessionTabs_Design.md §9.2、§12）：从会话列表拖出一个占位页签，
 * 验证能否从页面之外发起引擎原生的页签拖拽。控制台变量 Shuttlegent.Prototype.SessionDrag 为 0 时什么也不做。
 * 占位页签不承载会话内容；M3 正式实现后移除。
 */
struct FShuttlegentSessionDragPrototype
{
	/** 在主页签所在的栈中插入一个占位页签，等它取得几何信息后，若左键仍按下则交给原生页签拖拽。每一步都写日志。 */
	static void Begin(const TSharedPtr<SDockTab>& HubTab, const FString& SessionId, const FString& Title);
};
