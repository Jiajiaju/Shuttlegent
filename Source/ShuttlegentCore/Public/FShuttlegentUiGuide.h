#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentUiGuideKind.h"

#include "FShuttlegentUiGuide.generated.h"

/** 助手可以为用户打开的一个编辑器位置，附一句在那里做什么的说明（Shuttlegent.md 第 11 节）。 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentUiGuide
{
	GENERATED_BODY()

	/** 模型传给 show_editor_ui 的键，例如 world-settings。 */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	FString Id;

	/** 打开的位置类型：编辑器页签或设置页面。 */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	EShuttlegentUiGuideKind Kind = EShuttlegentUiGuideKind::Tab;

	/** Tab：页签 ID。Settings：容器/分类/分节，例如 Project/Engine/Rendering。 */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	FString Target;

	/** 用户在那里能找到或要做的事；会告知模型，并在打开时返回。 */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	FString Description;
};
