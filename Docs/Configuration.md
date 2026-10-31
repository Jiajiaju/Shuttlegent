# 配置

Shuttlegent 的设置分两处：

| 位置 | 谁来改 | 存放 |
|---|---|---|
| **编辑器偏好设置 > Shuttlegent > General** | 每个人自己 | 本机 `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`，不进版本库 |
| **项目设置 > Shuttlegent > General** | 项目负责人，全团队共享 | 项目 `Config/DefaultShuttlegent.ini`，随项目进版本库；插件 `Config/DefaultShuttlegent.ini` 提供默认值 |

另有两个提示词文件放在项目根目录：`SHUTTLEGENT.md`（项目提示词，团队共享）和 `SHUTTLEGENT.local.md`（个人提示词，不要提交）。

## 1. 模型与密钥（个人）

模型供应商的端点和 API 密钥、外部 MCP 服务请求头的密钥，**只从编辑器偏好设置读取**；不读 Windows 凭据管理器、环境变量或 `.env` 文件。密钥以明文保存在上表的个人 ini 中。

| 字段 | 默认值 | 说明 |
|---|---|---|
| Providers > DeepSeek Base Url | `https://api.deepseek.com/anthropic` | 内置 DeepSeek 路由的端点（Anthropic Messages 协议）；留空也用这个地址 |
| Providers > DeepSeek Api Key | 空 | 内置路由的密钥；默认路由缺密钥时运行时不启动，面板提示到这里填写 |
| Providers > Providers | 空 | 其他路由：OpenAI、Anthropic 协议或任何 OpenAI 兼容端点（团队网关、本地模型服务）。每个条目填自己的 Base Url 和 Api Key；新条目的 Base Url 默认 `https://api.deepseek.com` |
| Providers > Default Provider / Default Model | 空 | 新会话使用的路由和模型；留空用内置 DeepSeek |
| Credentials > Mcp Header Values | 空 | 变量名到值的映射，填项目设置中外部 MCP 服务请求头引用的变量 |

模型设置在运行时重启后生效（面板中点“重试”或重启编辑器）；MCP 请求头的值在新会话中生效。

## 2. 权限（个人）

| 字段 | 默认值 | 说明 |
|---|---|---|
| Permissions > Allow Workspace Write | 关 | 打开后，运行时的文件工具可以不经询问在项目目录内写文件；关闭时每次写文件都要提权 |
| Permissions > Allow Shell | 关 | 给运行时 PowerShell 工具 |
| Permissions > Allow Elevation | 开 | 允许运行时请求超出沙箱的权限；关闭时这类请求直接拒绝 |
| Permissions > Permission Policy | Reject | 没有编辑器对话框处理时，对运行时权限请求的默认回答 |

编辑器写操作（改属性、生成 Actor 等）不受这些开关影响，始终经确认卡，见 [Tools.md §1](Tools.md#1-写操作确认与撤销)。

## 3. 工具开关（个人）

- **Editor Tools**：每个编辑器工具一个开关，关闭后模型看不到也调用不了；立即生效。
- **Engine Toolsets**：每个已启用的引擎 Toolset 一个开关。项目已屏蔽的 Toolset 显示为锁定。

## 4. 运行时与外观（个人）

| 字段 | 默认值 | 说明 |
|---|---|---|
| Runtime > Runtime Executable Override | 空 | 用其他运行时 exe 代替插件的 `Runtime/shuttlegent-runtime.exe` |
| Runtime > Prewarm Runtime | 开 | 编辑器启动空闲后预先启动运行时，打开面板时已连接 |
| Appearance > Animate Mark | 开 | 面板中播放标志动画 |
| Appearance > Font Size | 13 | 对话与输入框字号（11–20） |
| Appearance > Wide Layout | 关 | 对话铺满两侧窗格之间的宽度 |

## 5. 项目设置（团队）

| 字段 | 说明 |
|---|---|
| Integration > Skill Dirs | 运行时加载的技能目录，相对项目目录 |
| Integration > Template Dirs | 斜杠模板（`*.md`）目录，相对项目目录 |
| Integration > External Mcp Servers | 每个会话都声明给运行时的外部知识或工具服务 |
| Integration > Ui Guides | Agent 可以帮用户打开的编辑器位置 |
| Engine Toolsets > Blocked Toolset Names | 全团队屏蔽的引擎 Toolset 或工具 |
| Engine Toolsets > Read Only Tool Prefixes / Read Only Tools / Write Tools | 引擎 Toolset 工具的读写判断规则 |
| Engine Toolsets > Non Undoable Tools | 不记入编辑器事务、标为不可撤销的引擎工具（撤销会损坏对象的工具），见 [Tools.md §3.3](Tools.md#33-撤销) |
| Engine Toolsets > Destructive Tool Prefixes | 名字以这些词开头的引擎工具，确认卡标为删除、移动或改名操作 |

外部 MCP 服务是否会声明给下一次会话、被跳过的原因，可在面板检视窗格的 MCP 卡片中查看（见 [Architecture.md §4](Architecture.md#4-对话面板)）。

引擎 Toolset 的规则与默认值见 [Tools.md §3](Tools.md#3-引擎-toolset)；技能、模板、外部 MCP 与界面指路的写法见 [Extending.md](Extending.md)。

⚠️ 插件的默认名单写在插件 `Config/DefaultShuttlegent.ini` 中。项目要增减条目时，在项目的 `Config/DefaultShuttlegent.ini` 里用 `+条目=` 追加、`-条目=` 删除，不要整段重写，否则默认规则会被覆盖。

```ini
[/Script/ShuttlegentCore.ShuttlegentProjectSettings]
+ReadOnlyTools=SomeQueryTool
-BlockedToolsetNames=PluginToolset
```
