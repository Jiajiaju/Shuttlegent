# 工具

Agent 通过工具查看和改动编辑器。工具有三类：

- **内置工具**：Shuttlegent 自带，覆盖关卡、资产、属性、材质、摆放等常用操作（§2）。
- **引擎 Toolset**：UE 5.8 各系统自带的 AI 工具，如 UMG、Niagara、PCG（§3）。
- **自定义工具**：项目自己写的工具，见 [Extending.md §6](Extending.md#6-自定义工具)。

## 1. 写操作确认与撤销

所有会改动编辑器的工具，都按同一套流程执行：

1. **预览**：先算出要改哪些对象、哪些字段、当前值和计划值，这一步不改任何东西。
2. **确认**：面板弹出确认卡，列出操作、对象、所在关卡、能否撤销。55 秒内没有确认视为放弃；面板没打开时直接拒绝。
3. **执行**：确认后在一个编辑器事务里执行；任一对象失败就整批回滚，不留半成品。
4. **结果**：对话里逐个对象列出成败，并附上这次执行期间 Output Log 里的警告与错误，Agent 据此判断是否真的成功。
5. **撤销**：结果旁有“撤销”按钮（这次改动仍在撤销栈顶时可用），也可以用编辑器的 Ctrl+Z。

是否弹确认卡，由**发起这次改动的会话**的模式决定：

| 模式 | 可撤销的改动 | 不可撤销的改动（存盘、导出文件等） | 删除、移动、改名 |
|---|---|---|---|
| 手动 | 确认卡 | 确认卡 | 确认卡并警告 |
| 自动接受编辑 | 直接执行 | 确认卡 | 确认卡并警告 |
| 自动 | 直接执行 | 直接执行 | 确认卡并警告 |
| 计划 | 拒绝 | 拒绝 | 拒绝 |

- 直接执行的改动照样出现在对话里，标“自动执行”，同样可以撤销。
- 计划模式下改动会被拒绝，Agent 会被告知先交计划、批准后再动手。
- 多个会话同时运行时互不影响：每张确认卡只出现在发起它的会话里，按那个会话的模式处理。

只读工具不弹确认卡，直接执行。

## 2. 内置工具

每个工具都可以在 **编辑器偏好设置 > Shuttlegent > General > Editor Tools** 里单独关闭。

**查看（只读）**

| 工具 | 作用 |
|---|---|
| `list_actors` | 列出当前关卡的 Actor（World Partition 只含已加载的区域） |
| `get_selection` | 读取关卡里当前选中的 Actor |
| `find_assets` | 按名称、类、文件夹搜索资产，不加载资产 |
| `get_asset` | 一个资产的类、包、标签，以及依赖与被引用数量 |
| `get_asset_references` | 资产依赖哪些包，或被哪些包引用 |
| `get_properties` | 读取 Actor、组件、资产或类默认对象的属性值 |
| `describe_class` | 类的继承链、可编辑属性、蓝图可调用函数 |
| `get_settings` | 列出或读取项目设置、编辑器偏好设置的各页 |
| `get_material_parameters` | 一次读出材质或材质实例的全部参数与父材质链；传 Actor 时列出各材质槽 |
| `read_log` | 读取编辑器启动以来的 Output Log |
| `get_cvar` | 读取控制台变量的值与说明，或按名称搜索 |

**和你交流（不改动编辑器）**

| 工具 | 作用 |
|---|---|
| `ask_user` | 请求有歧义时弹出选项让你选（2～6 项，可另填或跳过）；55 秒内没回答，Agent 改用文字提问 |
| `ask_choices` | 在对话里放一张选择卡（2～24 项，可带图、多选、写评语），适合挑图、配色、选材质、对比方案；提交后作为下一条消息发出 |
| `remember` / `forget` | 记下或删除一条个人记忆，见 [Extending.md §1](Extending.md#1-项目提示词)；不弹确认卡，对话里可撤销 |
| `show_editor_ui` | 帮你打开项目配置的编辑器位置，见 [Extending.md §4](Extending.md#4-界面指路) |

**改动（先确认）**

| 工具 | 作用 |
|---|---|
| `set_property` | 修改对象属性 |
| `spawn_actor` | 用类或蓝图在当前关卡生成 Actor |
| `delete_actors` | 从当前关卡删除 Actor |
| `place_actors` | 批量摆放：静态网格体、蓝图、类或复制已有 Actor；按网格、直线、圆周、样条、散布排列，可随机朝向与缩放、贴地、放进大纲文件夹；一次最多 500 个 |
| `arrange_actors` | 整理已有 Actor：对齐、等距分布、贴地、重新排列、随机朝向与缩放 |
| `set_material_parameters` | 多个材质实例的多个参数一次改完，一张确认卡逐行列出 |
| `create_material_instances` | 从一个父材质批量新建材质实例，可同时设参数 |
| `assign_material` | 给关卡里的 Actor 换材质（只改这个 Actor 的覆盖，不改网格体资产） |
| `tidy_graph` | 整理节点图排版：蓝图、动画蓝图、控件蓝图、材质、材质函数、Control Rig 等 |
| `export_data_table` | 把数据表导出为 CSV 或 JSON（CSV 用 Excel 打开中文不乱码），默认存到 `Saved/Shuttlegent/Exports/` |
| `save_assets` | 把资产或关卡保存到磁盘 |
| `undo` / `redo` | 撤销或重做编辑器操作，确认卡列出具体步骤 |

几点说明：

- **不能撤销的**：`save_assets`、`export_data_table`、`create_material_instances`（新建资产），确认卡上会标明。
- **摆放的位置就是确认卡上的位置**：`place_actors`、`arrange_actors` 预览时就算好每个物体的变换；随机散布没给种子时，用参数算出固定种子，执行结果和预览一致。贴地时下方没有地面的物体保持原高度，确认卡会注明个数。
- **`tidy_graph`** 按“数据从左往右”分列排版，可以只排指定节点，注释框跟着调整。材质已经在材质编辑器里打开时，排的是编辑器里的那份，需要在编辑器里点“应用”或保存才写回资产。
- **材质工具** 补的是引擎 `MaterialTools`、`MaterialInstanceTools` 缺少的批量能力；编辑材质节点图仍用引擎工具。

## 3. 引擎 Toolset

UE 5.8 的 Toolset Registry 把各系统的 AI 工具做成独立的 Toolset 插件：UMG、Niagara、PCG、GameplayTags、StateTree、物理、动画等。Shuttlegent 用三个工具把已启用的 Toolset 交给 Agent，Agent 先查再用，不必一次读入几百个工具：

| 工具 | 作用 |
|---|---|
| `list_toolsets` | 列出可用的 Toolset 及工具数 |
| `describe_toolset` | 列出一个 Toolset 的工具并标注读写；给出工具名时返回完整说明与参数 |
| `call_toolset_tool` | 调用一个工具；只读的直接执行，其余先弹确认卡 |

### 3.1 启用 Toolset

每个 Toolset 是单独的引擎插件，默认不启用。在 **编辑 > 插件** 里搜索 Toolset，按需启用后重启编辑器。面向策划与美术，建议启用：

EditorToolset、UMGToolSet、MVVMToolset、NiagaraToolsets、PCGToolset、GameplayTagsToolset、DataRegistryToolset、GASToolsets、PhysicsToolsets、ChaosClothAssetToolset、DataflowAgent、StateTreeToolset、AnimationAssistantToolset、ConversationToolset、AIModuleToolset、SemanticSearchToolset。

### 3.2 读写判断

引擎工具本身不标注读写，Shuttlegent 按下面的顺序判断（规则在 **项目设置 > Shuttlegent > General > Engine Toolsets**）：

1. 列在 Write Tools 里的，算改动；
2. 列在 Read Only Tools 里的，算只读；
3. 名字以 Read Only Tool Prefixes 里某个词开头的，算只读。默认词为 `Get`、`List`、`Find`、`Search`、`Describe`、`Is`、`Has`、`Query`、`Discover`、`Read`；词后面必须是名字结尾、大写字母或下划线，如 `GetActors`、`get_actors`；
4. 其余一律算改动，先确认。

### 3.3 撤销

引擎工具在编辑器事务中执行：

- 引擎记录了改动的，可以撤销；
- 没有记录的（如新建资产、保存文件），标为不可撤销；
- 结果异步返回的，标为“仍在后台执行”；
- 列在 **项目设置 > Engine Toolsets > Non Undoable Tools** 里的，不记入事务，标为不可撤销。默认包括 DataTableTools 的 `add_rows`、`set_rows`、`rename_rows`：它们会重建整张表，撤销重建会销毁表对象。

工具返回 `{"returnValue": false}` 时按失败处理。

### 3.4 屏蔽

**项目设置 > Blocked Toolset Names** 对全团队屏蔽 Toolset 或单个工具：按名称包含匹配（不分大小写），用 `/.../` 包起来时按正则匹配；匹配对象是 Toolset 名或 `Toolset.工具` 全名。个人也可以在 **编辑器偏好设置 > Shuttlegent > General > Engine Toolsets** 里关掉单个 Toolset。

插件默认屏蔽：

| 类别 | 屏蔽项 | 原因 |
|---|---|---|
| 写代码与测试 | LiveCodingToolset、AutomationTestToolset | 不在插件定位内 |
| 插件与功能开关 | PluginToolset、GameFeaturesToolset | 改动项目结构，风险高 |
| 界面模拟 | SlateInspectorToolset | 能模拟点击编辑器界面 |
| 外部 MCP 客户端 | MCPClientToolset | Shuttlegent 自己管理外部服务接入 |
| 截图类工具 | 名为 `Screenshot`，或以 `Capture` 开头的（下面三个除外） | 结果未逐个核实 |

`EditorAppToolset` 的三个截图工具不屏蔽，按只读直接执行：`CaptureViewport`（关卡视口，可叠加网格坐标与 Actor 标注）、`CaptureEditorImage`（整个编辑器窗口）、`CaptureAssetImage`（资产缩略图）。截图最长边超过 1,600 像素时先缩小，保存在 `Saved/Shuttlegent/Captures/`，并显示在对话里；模型不支持图片时，Agent 只拿到文字说明。

### 3.5 引擎提示框

引擎工具执行时，引擎内部可能弹出模态提示框，卡住编辑器直到有人点击。Shuttlegent 调用引擎工具期间：

- 提示框不弹出，按默认答案处理：只有“确定”的直接关闭；“确定/取消”“是/否”取默认值，多为取消或否；
- 执行期间的警告、错误与提示框文字附在工具结果后面交给 Agent，Agent 据此修正参数或向你说明；
- 工具把工作推迟到之后的帧执行时，那部分仍可能弹框。

### 3.6 破坏性操作

名字以 **项目设置 > Engine Toolsets > Destructive Tool Prefixes** 里某个词开头的引擎工具（默认 `Delete`、`Remove`、`Destroy`、`Move`、`Rename`），以及内置的 `delete_actors`，确认卡顶部会显示醒目提示，提醒你逐个核对对象。
