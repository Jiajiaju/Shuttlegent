#pragma once

#include "CoreMinimal.h"

/** 内置编辑器工具共用的限制值（Shuttlegent.md 第 10.2 节）。 */
namespace ShuttlegentToolsConstant
{
	// 每个属性显示的属性值最大长度（字符数）；超出部分以省略号结尾
	inline constexpr int32 MaxValueChars = 400;

	// get_asset 最多显示的资源标签数
	inline constexpr int32 MaxAssetTags = 40;

	// 自编辑器启动以来为 read_log 保留的日志行数，以及其默认返回行数与最大返回行数
	inline constexpr int32 LogCapacity = 5000;
	inline constexpr int32 DefaultLogLines = 100;
	inline constexpr int32 MaxLogLines = 500;

	// 单次调用最多执行的撤销与重做步数
	inline constexpr int32 MaxHistorySteps = 20;

	// ask_user 提供答案数的下限与上限；面板将其映射到数字键 1-6
	inline constexpr int32 MinQuestionOptions = 2;
	inline constexpr int32 MaxQuestionOptions = 6;

	// 对象路径或包路径的首字符；不以该字符开头的名称视为 Actor 标签
	inline const TCHAR* PathPrefix = TEXT("/");

	// find_assets 默认搜索的包路径
	inline const TCHAR* DefaultAssetRoot = TEXT("/Game");

	// 模型可能写出、而 UE 类名中省略的类名前缀（AStaticMeshActor -> StaticMeshActor）
	inline const TCHAR* ClassPrefixes[] = { TEXT("A"), TEXT("U") };

	// Blueprint 生成类的后缀
	inline const TCHAR* GeneratedClassSuffix = TEXT("_C");

	// 截断至 MaxValueChars 的值后附加的显示内容
	inline const TCHAR* Ellipsis = TEXT("…");

	// Details 面板分类的属性元数据键
	inline const TCHAR* MetaCategory = TEXT("Category");

	// bool 属性显示的类型；C++ 中 bool 属性声明为 uint8 位域与声明为 bool 同样常见
	inline const TCHAR* BoolTypeName = TEXT("bool");
}
