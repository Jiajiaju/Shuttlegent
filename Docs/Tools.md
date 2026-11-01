# 工具

Agent 通过编辑器内的 MCP 端点使用编辑器工具。工具分三类：Shuttlegent 内置工具、引擎 Toolset 工具，以及项目自定义工具（见 [Extending.md §6](Extending.md#6-自定义工具)）。

## 1. 写操作确认与撤销

所有会改动编辑器状态的工具都经过同一道写入门：

1. **预览**：工具先列出要改的对象、字段、当前值和计划值，不做任何修改。
2. **确认**：面板弹出确认卡，显示操作、对象表、所在关卡、能否撤销和原始参数。55 秒内未确认视为放弃；面板没打开时直接拒绝。计划模式下有回合进行中时，写入不弹确认卡、直接拒绝，并告诉 Agent 先交计划、批准后才能写（`FShuttlegentWriteGate::IsWriteBlocked`，决定为 `Blocked`）。写入请求不带会话，因此只要有处于计划模式的对话在运行，所有写入都会被拒绝。
3. **执行**：确认后在一个编辑器事务里执行。任一对象失败时整批回滚，不留半成品。
4. **结果**：执行结果逐个对象列出成败，并附上这次执行期间 Output Log 中的警告与错误（最多 20 条，每条最多 300 字符）。失败或回滚时 Agent 不必再调用 `read_log` 就能看到原因；写入报告成功但日志里有错误时，结果会提醒 Agent 先核对再告诉用户成功了。全部写入且没有警告时，结果说明这就是编辑器实际写入的值，至多读回一次确认，避免反复读取或重写。
5. **撤销**：执行结果显示在面板结果栏，可以一键撤销（该事务仍在撤销栈顶时）；也可以用编辑器的 Ctrl+Z。

只读工具不弹确认卡，直接执行。

## 2. 内置工具

| 分组 | 工具 | 读写 | 作用 |
|---|---|---|---|
| 关卡 | `list_actors` | 读 | 列出当前关卡的 Actor（World Partition 只含已加载的单元） |
| 关卡 | `get_selection` | 读 | 读取关卡编辑器中当前选中的 Actor |
| 资源 | `find_assets` | 读 | 按名称、类、文件夹搜索资源，不加载资源 |
| 资源 | `get_asset` | 读 | 一个资源的类、包、注册表标签，以及依赖与被引用的数量 |
| 资源 | `get_asset_references` | 读 | 资源依赖的包，或引用它的包 |
| 反射 | `get_properties` | 读 | 读取 Actor、组件、资源或类默认对象的属性值 |
| 反射 | `describe_class` | 读 | 类的继承链、可编辑属性及类型、蓝图可调用函数 |
| 反射 | `get_settings` | 读 | 列出项目设置与编辑器偏好设置的各页，或读取某页的当前值 |
| 诊断 | `read_log` | 读 | 读取编辑器启动以来的 Output Log |
| 诊断 | `get_cvar` | 读 | 读取控制台变量的值与说明，或按名称搜索控制台变量与命令 |
| 交互 | `ask_user` | 读 | 请求有歧义时，在面板弹出选项卡让用户选择（2～6 个选项，可另填答案或跳过），返回用户的回答；限时 55 秒，超时或面板未打开时 Agent 改用文字提问。不改动编辑器 |
| 界面指路 | `show_editor_ui` | 读 | 为用户打开项目配置的编辑器位置（见 [Extending.md §4](Extending.md#4-界面指路)） |
| 编辑 | `set_property` | 写 | 修改对象属性 |
| 编辑 | `spawn_actor` | 写 | 用原生类或蓝图在当前关卡生成 Actor |
| 编辑 | `delete_actors` | 写 | 从当前关卡删除 Actor |
| 编辑 | `save_assets` | 写 | 把资源或关卡保存到磁盘；保存不能撤销 |
| 材质 | `get_material_parameters` | 读 | 一次读出材质或材质实例的全部参数（分组、类型、当前值、是否覆盖父材质）与父材质链；传 Actor 时列出各网格体组件每个材质槽的材质 |
| 材质 | `set_material_parameters` | 写 | 多个材质实例的多个参数一次改完，一张确认卡逐行列出当前值与计划值；改静态开关会重新编译材质；不改材质本体 |
| 材质 | `create_material_instances` | 写 | 从一个父材质批量新建材质实例，可同时设置参数；同名资源已存在时拒绝；新建资源不能撤销，保存需另行调用 `save_assets` |
| 材质 | `assign_material` | 写 | 给关卡中的 Actor 换材质（组件上的材质覆盖），可指定材质槽；网格体资产与其他摆放不受影响 |
| 节点图 | `tidy_graph` | 写 | 整理节点图排版：蓝图（含动画蓝图、控件蓝图的事件图、函数与宏）、材质与材质函数、Control Rig，以及其他以 UEdGraph 保存节点的资源。可只排指定节点，其余节点原地不动；注释框随所框节点调整；可撤销 |
| 关卡搭建 | `place_actors` | 写 | 批量新建 Actor：静态网格体、蓝图、类，或复制关卡中已有的 Actor；按网格、直线、圆周、样条、散布摆放，可随机朝向与缩放、贴地、放进大纲文件夹；单次最多 500 个，一张确认卡逐个列出变换 |
| 关卡搭建 | `arrange_actors` | 写 | 整理已有 Actor：沿轴对齐、等距分布、贴地、按网格/直线/圆周/样条/散布重新排列、随机朝向与缩放；确认卡逐个列出当前与计划变换 |
| 数据表 | `export_data_table` | 写 | 把数据表导出为 CSV 或 JSON 文件，格式与编辑器的数据表导入一致；CSV 为带 BOM 的 UTF-8，Excel 打开中文不乱码；默认存到 `Saved/Shuttlegent/Exports/`，只能写在项目目录内；覆盖已有文件前确认卡会警告；写文件不能撤销 |
| 历史 | `undo` / `redo` | 写 | 撤销或重做编辑器操作，确认卡列出具体步骤 |

每个工具都可以在 **编辑器偏好设置 > Shuttlegent > General > Editor Tools** 中单独关闭。

关卡搭建工具在预览时只计算变换，不生成也不移动任何东西；执行时按同样的参数重新计算并写入。散布与随机化没给 `seed` 时，由全部参数算出固定种子，确认卡上的位置就是最终位置。贴地从轴心上方向下做射线检测（`ECC_Visibility`），让物体底部落在命中点上；下方没有地面的保持原高度，确认卡的操作文字会注明个数。

`tidy_graph` 的排版规则：数据从左往右分列，列内按引脚顺序排行，互不相连的几组上下叠放，没有连线的节点排在最左一列（只排部分节点时放到其余节点下方）。节点尺寸在资源编辑器打开时取界面实际绘制的大小，否则为估算，确认卡会注明。材质已在材质编辑器中打开时，排的是编辑器的工作图，需在编辑器中点“应用”或保存才写回资源；未打开时直接改资源。Control Rig 的节点位置经 RigVM 控制器写入，与其他写入一样随编辑器事务撤销。

材质工具只补引擎 `MaterialTools`、`MaterialInstanceTools` 缺的批量与换材质能力，引擎工具照常可用：编辑材质节点图用 `MaterialTools`；单个参数也可用 `MaterialInstanceTools`，但它每改一个参数弹一张确认卡，且确认卡只显示原始参数。内置材质工具的说明引导 Agent 批量操作时优先用内置工具。

## 3. 引擎 Toolset

UE 5.8 的 Toolset Registry 把各系统的 AI 工具做成独立的 Toolset 插件：UMG、Niagara、PCG、GameplayTags、StateTree、物理、动画等。Shuttlegent 依赖 `ToolsetRegistry` 插件，用三个工具把已启用的 Toolset 交给 Agent：

| 工具 | 作用 |
|---|---|
| `list_toolsets` | 列出可用的 Toolset 及工具数 |
| `describe_toolset` | 列出一个 Toolset 的工具并标注读写；传入工具名时给出完整说明与参数 |
| `call_toolset_tool` | 调用一个工具；只读工具直接执行，其余先弹确认卡 |

Agent 先查再用，不必一次读入几百个工具。

### 3.1 启用 Toolset

每个 Toolset 是单独的引擎插件，默认不启用。在 **编辑 > 插件** 中搜索 Toolset，按需启用后重启编辑器。面向策划与美术，建议启用：

EditorToolset、UMGToolSet、MVVMToolset、NiagaraToolsets、PCGToolset、GameplayTagsToolset、DataRegistryToolset、GASToolsets、PhysicsToolsets、ChaosClothAssetToolset、DataflowAgent、StateTreeToolset、AnimationAssistantToolset、ConversationToolset、AIModuleToolset、SemanticSearchToolset。

### 3.2 读写判断

引擎工具本身不标记读写，Shuttlegent 按以下顺序判断（规则在 **项目设置 > Shuttlegent > General > Engine Toolsets**）：

1. 在 Write Tools 中的，按写操作处理；
2. 在 Read Only Tools 中的，按只读处理；
3. 名字以 Read Only Tool Prefixes 中某个词开头的，按只读处理。词后面必须是名字结尾、大写字母或下划线，如 `GetActors`、`get_actors`。默认词为 `Get`、`List`、`Find`、`Search`、`Describe`、`Is`、`Has`、`Query`、`Discover`、`Read`（引擎以 Read 开头的 `read_graph_dsl`、`read_file`、`ReadLogLines` 均只读）；
4. 其余一律按写操作处理，先确认。

### 3.3 撤销

引擎工具在编辑器事务中执行：

- 引擎记录了改动的，可以撤销；
- 没有记录的（如新建资源、保存文件），标为不可撤销；
- 结果异步返回的，标为“仍在后台执行”，不等待结果；
- 在 **项目设置 > Engine Toolsets > Non Undoable Tools** 中的，不记入事务，确认卡与结果都标为不可撤销。默认包括 DataTableTools 的 `add_rows`、`set_rows`、`rename_rows`：它们在原位置重建整张表，撤销这次重建会销毁表对象。

其余工具的确认卡标题会注明“能否撤销以执行结果为准”。

写操作返回 `{"returnValue": false}` 时按失败处理：资源类工具（如删除、改名）以返回 false 表示没有执行。

### 3.4 屏蔽

**项目设置 > Blocked Toolset Names** 对全团队屏蔽 Toolset 或单个工具。规则写法：按大小写不敏感的子串匹配；用 `/.../` 包起来时按正则匹配，匹配对象是 Toolset 名或 `Toolset.工具` 全名。

插件默认屏蔽：

| 类别 | 屏蔽项 | 原因 |
|---|---|---|
| 写代码与测试 | LiveCodingToolset、AutomationTestToolset | 不在插件定位内 |
| 插件与功能开关 | PluginToolset、GameFeaturesToolset | 改动项目结构，风险高 |
| 界面模拟 | SlateInspectorToolset | 能模拟点击编辑器界面 |
| 外部 MCP 客户端 | MCPClientToolset | Shuttlegent 自有外部 MCP 接入 |
| 截图类工具 | 名字以 `Capture` 开头或为 `Screenshot` 的工具 | 图片结果暂不交给模型 |

个人还可以在 **编辑器偏好设置 > Shuttlegent > General > Engine Toolsets** 中关掉单个 Toolset。

### 3.5 引擎提示框

引擎工具执行时，引擎内部可能弹出模态提示框（如参数不对时的“Cannot create a blueprint based on the class ...”），提示框会卡住编辑器直到有人点击。Shuttlegent 调用引擎工具期间按无人值守方式运行：

- 提示框不弹出，按默认答案处理：只有“确定”的直接关闭；“确定/取消”“是/否”等取默认值，多为取消或否；
- 工具执行期间引擎在游戏线程输出的警告、错误与提示框文字（最多 20 条）附在工具结果之后交给 Agent，Agent 据此修正参数或向用户说明；
- 只覆盖工具同步执行的部分；工具把工作推迟到之后的帧执行时，那部分仍可能弹框。

### 3.6 破坏性操作

名字以 **项目设置 > Engine Toolsets > Destructive Tool Prefixes** 中某个词开头的引擎工具（默认 `Delete`、`Remove`、`Destroy`、`Move`、`Rename`），以及内置的 `delete_actors`，确认卡顶部显示醒目提示，提醒逐个核对对象。
