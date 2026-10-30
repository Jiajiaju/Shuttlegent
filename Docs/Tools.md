# 工具

Agent 通过编辑器内的 MCP 端点使用编辑器工具。工具分三类：Shuttlegent 内置工具、引擎 Toolset 工具，以及项目自定义工具（见 [Extending.md §6](Extending.md#6-自定义工具)）。

## 1. 写操作确认与撤销

所有会改动编辑器状态的工具都经过同一道写入门：

1. **预览**：工具先列出要改的对象、字段、当前值和计划值，不做任何修改。
2. **确认**：面板弹出确认卡，显示操作、对象表、所在关卡、能否撤销和原始参数。55 秒内未确认视为放弃；面板没打开时直接拒绝。
3. **执行**：确认后在一个编辑器事务里执行。任一对象失败时整批回滚，不留半成品。
4. **撤销**：执行结果显示在面板结果栏，可以一键撤销（该事务仍在撤销栈顶时）；也可以用编辑器的 Ctrl+Z。

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
| 界面指路 | `show_editor_ui` | 读 | 为用户打开项目配置的编辑器位置（见 [Extending.md §4](Extending.md#4-界面指路)） |
| 编辑 | `set_property` | 写 | 修改对象属性 |
| 编辑 | `spawn_actor` | 写 | 用原生类或蓝图在当前关卡生成 Actor |
| 编辑 | `delete_actors` | 写 | 从当前关卡删除 Actor |
| 编辑 | `save_assets` | 写 | 把资源或关卡保存到磁盘；保存不能撤销 |
| 历史 | `undo` / `redo` | 写 | 撤销或重做编辑器操作，确认卡列出具体步骤 |

每个工具都可以在 **编辑器偏好设置 > Shuttlegent > General > Editor Tools** 中单独关闭。

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
3. 名字以 Read Only Tool Prefixes 中某个词开头的，按只读处理。词后面必须是名字结尾、大写字母或下划线，如 `GetActors`、`get_actors`。默认词为 `Get`、`List`、`Find`、`Search`、`Describe`、`Is`、`Has`、`Query`、`Discover`；
4. 其余一律按写操作处理，先确认。

### 3.3 撤销

引擎工具在编辑器事务中执行：

- 引擎记录了改动的，可以撤销；
- 没有记录的（如新建资源、保存文件），标为不可撤销；
- 结果异步返回的，标为“仍在后台执行”，不等待结果。

确认卡标题会注明“能否撤销以执行结果为准”。

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
