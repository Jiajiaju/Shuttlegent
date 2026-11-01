---
name: tag-review
description: 整理 GameplayTag：找出没有资源引用的标签、大小写或分隔不统一的标签、意思重复的标签。只汇报，删除或改名要逐个得到用户同意。用户要求整理、清理、检查 GameplayTag 时使用。
---

# GameplayTag 整理

引擎的 GameplayTag 工具在名字含 GameplayTags 的 Toolset 中；先用 `list_toolsets` 找到，再用 `describe_toolset` 查看参数。

## 步骤

1. 用 `ListTags` 读出标签（用户指定了范围，如 `Character.State`，就传这个父标签；否则传空字符串读全部）。超过 300 个时先问用户只查哪一支。
2. 逐项检查：

| 检查项 | 判断方法 |
|---|---|
| 写法不统一 | 同一层级里大小写风格不同（`Character.State` 与 `character.state`），或用了下划线、空格代替点分层 |
| 意思重复 | 同一父标签下名字只差单复数、缩写或同义词，如 `Weapon.Sword` 与 `Weapon.Swords` |
| 没有资源引用 | 对候选标签调用 `FindReferencersByTag`，结果为空 |

3. 对“没有资源引用”的标签，再用 `GetTagInfo` 看来源与说明：来源是代码（原生标签）或被其他标签当作父标签的，不要列为可删除。
4. 汇总成一张表：标签｜问题｜引用数｜来源｜建议。
5. 问用户要处理哪些。**删除与改名都要用户逐个明确同意**：
   - 改名用 `RenameTag`，引擎会同时更新资源中的引用；
   - 删除用 `RemoveTag`；
   - 两者都会改动标签所在的配置文件或数据表，提醒用户签出并提交。

## 注意

- `FindReferencersByTag` 只能查到资源中的引用，C++ 代码与配置文件中按名字使用的标签查不到；“没有资源引用”不等于“没用到”，在汇报中写明这一点。
- 一次只处理用户同意的那几个标签，不要顺手整理其他标签。
