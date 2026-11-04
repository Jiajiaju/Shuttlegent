#pragma once

#include "CoreMinimal.h"

class IPropertyHandle;
class SWidget;

/** 设置页中按名称选模型、按标识选 Agent 的下拉框，以及 Agent 的显示文字；默认值与 Agent 条目共用。 */
struct FShuttlegentSettingsChoices
{
	/** 模型名称下拉：列出名称与模型 ID 都已填写的模型；EmptyLabel 非空时第一项为它，选中即清空。 */
	static TSharedRef<SWidget> MakeModelCombo(const TSharedRef<IPropertyHandle>& Handle, const FText& EmptyLabel);

	/** Agent 下拉：列出已启用的 Agent，显示名称、保存标识。 */
	static TSharedRef<SWidget> MakeAgentCombo(const TSharedRef<IPropertyHandle>& Handle);

	/** Agent 在设置页与面板中显示的名称。 */
	static FText GetAgentLabel(const FString& AgentId);

	/** Agent 声明的能力，一行文字。 */
	static FText GetAgentCapabilities(const FString& AgentId);
};
