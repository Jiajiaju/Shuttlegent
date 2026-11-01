---
name: material-edit
description: 批量改材质实例的参数（颜色、粗糙度、贴图、开关），或给关卡里的物体换材质。用户要求“把这些材质颜色改成”“调粗糙度”“换成某某材质”时使用。
---

# 批量改材质

## 步骤

1. 确定对象：
   - 用户说材质实例时，用 `find_assets`（`class` 填 `MaterialInstanceConstant`）或用户给的路径；
   - 用户说关卡里的物体时，先用 `get_material_parameters` 传 Actor，读出它们各材质槽用的材质。
2. 用 `get_material_parameters` 读出参数名、类型和当前值。用户说的“颜色”“粗糙度”对应哪个参数不确定，或有多个候选时，用 `ask_user` 让用户选。
3. 按需求选工具：

| 需求 | 工具 |
|---|---|
| 改材质实例的参数 | `set_material_parameters`：所有实例和参数放进一次调用 |
| 改的是材质本体 | 不直接改：问用户是否用 `create_material_instances` 新建实例，再用 `assign_material` 换到物体上 |
| 只给某些物体换材质 | `assign_material`，只改这些物体，不动网格体资产 |
| 新建一批材质实例 | `create_material_instances`，参数一起设好 |

4. 值的写法：颜色写 `[r, g, b]`（0～1 的线性值）或 `"#RRGGBB"`；数值写数字；贴图写资源路径。
5. 复查：再调用一次 `get_material_parameters`，确认新值已生效。
6. 汇报：对象｜参数｜原值｜新值。改了静态开关时提醒用户材质会重新编译，可能要等一会儿。

## 注意

- 不要用 `set_property` 去改材质参数，也不要逐个参数地调用引擎的材质实例工具。
- 不保存；用户要求时再用 `save_assets`。
