---
name: level-review
description: 检查当前打开的关卡并汇总问题：默认名称的 Actor、缺材质或用默认材质的网格体、光源数量、重复摆放。只汇报不修改。用户要求检查、自查、体检关卡时使用。
---

# 关卡自查

只读取、不修改。发现问题后列出建议，由用户决定要不要处理。

## 步骤

1. 用 `list_actors` 读出关卡中的 Actor；结果分页时用 `offset` 读完全部。Actor 多于 500 个时只查前 500 个，并在结论中说明。
2. 逐项检查：

| 检查项 | 判断方法 |
|---|---|
| 默认名称 | 标签以类名开头，如 `StaticMeshActor_12`、`PointLight2`、`Cube`；用户自己起的名字不算 |
| 缺材质 | 对静态网格体 Actor 调用 `get_material_parameters`（一次传多个 Actor），材质槽为 None，或是 `WorldGridMaterial`、`DefaultMaterial` 这类引擎默认材质 |
| 光源数量 | 按类统计 PointLight、SpotLight、RectLight、DirectionalLight、SkyLight；DirectionalLight 或 SkyLight 多于 1 个时提醒 |
| 重复摆放 | 同类、位置相差不到 1 cm 的 Actor，多半是误复制 |

3. 汇总成一张表：检查项｜结果｜建议。每个问题对象都写成可点的对象链接；同一项超过 10 个时列前 10 个并写出总数。
4. 最后问一句要不要处理其中某项，例如给默认名称的 Actor 改名、删除重复的 Actor。用户同意后再动手：改名用 `call_toolset_tool` 调用引擎 `editor_toolset.toolsets.actor.ActorTools` 的 `set_label`（不要用 `set_property` 直接改标签字段），删除用 `delete_actors`。

## 注意

- 不要为了检查而保存、移动或删除任何东西。
- 某项因为工具读不到而没法检查时，在表里写“未检查”并说明原因，不要猜。
