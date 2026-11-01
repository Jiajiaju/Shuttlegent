---
name: table-edit
description: 修改数据表（DataTable）：加行、改行、删行、改名，或导出给用户用 Excel 改完再导回。用户要求改表、加数据、调数值表时使用。先导出备份，因为引擎的改表工具不能撤销。
---

# 批量改表

引擎 `editor_toolset.toolsets.data_table.DataTableTools` 的加行、改行、改名、导入都会整表重建，**不能撤销**，所以动手前一定要先导出备份。

## 步骤

用户只要导出（例如“导出成 CSV 我自己用 Excel 改”）、这次不改表时，只调用一次 `export_data_table`，告诉用户文件路径即可，不做备份、不读行；用户改好要导回时再按下面的步骤做。


1. 用 `find_assets`（`class` 填 `DataTable`）找到表，表名不明确时用 `ask_user` 确认。
2. 用 `describe_toolset` 查看 DataTableTools 的工具与参数，再用 `call_toolset_tool` 调用 `get_schema`、`list_rows`、`get_rows` 读出列和现有的行。
3. **备份**：调用 `export_data_table`，`path` 写 `Saved/Shuttlegent/Exports/<表名>.before.csv`。备份失败就停下来，不要继续改。
4. 按改动规模选做法：
   - 改动不超过 20 行：直接用引擎工具改，同一种操作的多行合并成一次调用。加行分两步：`add_rows` 只加出空行，再用一次 `set_rows` 填入各列的值；改值只用 `set_rows`；删行用 `remove_rows`，改名用 `rename_rows`。新行的列值用户没说时用 `ask_user` 问，不要自己编；
   - 改动更多，或用户想自己在 Excel 里改：调用 `export_data_table` 导出（默认位置即可），告诉用户文件路径，等用户改好、明确说可以导入后，再用 `import_file` 导回。
5. 复查：用 `get_rows` 读出改过的行，与要求逐项核对；不要为了复查再导出文件。
6. 汇报改了哪些行、哪些列，以及备份文件的位置。

## 注意

- 不要对表做撤销（Ctrl+Z）：会销毁表对象。改错了就从备份导回。
- 工具报错表“已被销毁”或无法加载时，立即停下来告诉用户，**不要**删除、移动、复制或重建这张表来“修复”。
- 不保存；用户要求时再用 `save_assets`。
