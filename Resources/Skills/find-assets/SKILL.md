---
name: find-assets
description: 用户描述了一个资源但没给准确路径时（如“英雄的模型”“那个木箱”），先查清是哪个资源、被哪里用到，再继续任务。适用于找资源、查资源用在哪、确认改动影响范围。
---

# 先查清再动手

## 步骤

1. 把用户的描述拆成可能出现在资源名里的英文词，例如“英雄”→ Hero、“木箱”→ Crate、Box、“门”→ Door；再按资源类型选 `class`：静态网格体 `StaticMesh`、骨骼网格体 `SkeletalMesh`、蓝图 `Blueprint`、材质 `Material`、材质实例 `MaterialInstanceConstant`、数据表 `DataTable`、关卡 `World`。
2. 用 `find_assets` 逐个试；结果太多时加 `path` 限定文件夹，结果为空时换一个词。
3. 候选不止一个时，用 `get_asset` 看各自的类与标签，仍分不清就用 `ask_user` 让用户选，不要自己挑一个。
4. 用户关心影响范围，或接下来要修改这个资源时，用 `get_asset_references`：`direction` 填 `referencers` 查谁用到它，填 `dependencies` 查它用到什么。
5. 回答时写出资源的完整路径（写成可点的对象链接），以及被引用的情况；用户还有后续要求时再接着做。

## 注意

- 这一步只查不改。
- 关卡里的物体用 `list_actors` 查，不要用 `find_assets`。
