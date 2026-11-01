---
name: umg-review
description: 检查控件蓝图（UMG）的命名与层级：默认名称的控件、嵌套过深的层级、被设为变量的控件。只汇报，用户确认后再改名或调整。用户要求检查界面、UI 命名、控件层级时使用。
---

# UMG 命名与层级检查

引擎的 UMG 工具在名字含 UMG 的 Toolset 中；先用 `list_toolsets` 找到，再用 `describe_toolset` 查看参数。

## 步骤

1. 确定要查的控件蓝图：用户给了路径就用；否则用 `ListWidgetBlueprints`（传内容文件夹，如 `/Game/UI`）或 `find_assets`（`class` 填 `WidgetBlueprint`）。超过 10 个时先问用户只查哪些。
2. 对每个控件蓝图调用 `GetWidgets`，拿到控件树（每个控件的名称、类型、父子关系、是否为变量）。
3. 逐项检查：

| 检查项 | 判断方法 |
|---|---|
| 默认名称 | 名称是“类型名_数字”，如 `TextBlock_0`、`Button_23`、`Image_7`、`CanvasPanel_1` |
| 层级过深 | 从根到某个控件经过的容器超过 8 层；只套了一个子控件的容器多半可以去掉 |
| 变量 | 被设为变量、而名字仍是默认名称的控件，多半是误勾；名字有意义的变量通常在图表中用到，不要建议取消 |

4. 汇总成一张表：控件蓝图｜控件｜问题｜建议（给出建议的新名字，按项目已有的命名习惯，例如 `Txt_Title`、`Btn_Confirm`）。
5. 问用户要处理哪些。用户同意后：
   - 改名用 `RenameWidget`，改完用 `CompileWidgetBlueprint` 编译；
   - 取消变量用 `ToggleWidgetAsVariable`，之前先用引擎蓝图工具的 `find_nodes` 确认图表中没有用到它；
   - 调整层级（如 `ReplaceWidgetWithChild` 去掉多余容器）属于结构改动，先说明会发生什么，再逐个确认。

## 注意

- 只汇报阶段不做任何修改。
- 不保存；用户要求时再用 `save_assets`。
