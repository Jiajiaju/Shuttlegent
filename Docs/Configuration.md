# 配置

本文讲 Shuttlegent 的各项设置，给使用者和项目负责人看。项目规则、技能等扩展的写法见 [Extending.md](Extending.md)。

设置分两处：

| 位置 | 谁来改 | 存在哪 |
|---|---|---|
| **编辑器偏好设置 > Shuttlegent > General** | 每个人自己 | 本机 `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`，不进版本库 |
| **项目设置 > Shuttlegent > General** | 项目负责人，全团队共享 | 项目 `Config/DefaultShuttlegent.ini`，随项目进版本库 |

第一次使用只需要做一件事：给默认模型填密钥（§1）。其余设置都有默认值，按需再改。

**个人文件**都在项目的 `Saved/Shuttlegent/` 下，不进版本库：

| 文件或目录 | 内容 |
|---|---|
| `SHUTTLEGENT.local.md` | 个人提示词，见 [Extending.md §1](Extending.md#1-提示词与记忆) |
| `MEMORY.md` | 个人记忆，见 [Extending.md §1](Extending.md#1-提示词与记忆) |
| `Sessions/` | 会话记录、会话列表、草稿与排队消息、发过的图片 |
| `Captures/`、`Exports/` | Agent 的截图、数据表导出 |

团队共享的项目提示词 `SHUTTLEGENT.md` 放在项目根目录。

## 1. 模型、Agent 与默认值（个人）

**密钥只从这里读取**，不读 Windows 凭据管理器、环境变量或 `.env` 文件；密钥以明文保存在上表的个人 ini 中。

| 字段 | 默认值 | 什么时候改 |
|---|---|---|
| Models | 一条内置的 DeepSeek 连接 | 填密钥；想用其他服务时添加一条连接，只填 Base Url 与 Api Key |
| Agents | 默认 | 启用或停用某个 Agent；给 Agent 指定默认模型 |
| Defaults > Default Agent | 默认 | 新会话想默认用别的 Agent |
| Credentials > Mcp Header Values | 空 | 项目接入的外部服务需要个人密钥时，按项目负责人给的变量名填写 |

- **在面板里直接连接**：一条连通的连接都没有时，面板会显示“连接一个模型”卡片。选 DeepSeek 只填密钥，选其他服务再填地址，点“连接”后自动写进上面的 Models 并检测，连通即可开始对话；没连通会提示，改好再点一次。
- **一条连接带出全部模型**：每条连接只填地址（如 `https://api.openai.com/v1`）和密钥。插件自动识别协议（OpenAI Responses、OpenAI Chat Completions 或 Anthropic Messages），并列出这组地址和密钥能用的全部对话模型，最新的在前；它们都会出现在面板的模型菜单里。
- **默认模型自动选**：第一条已连通的连接中的第一个模型。想换别的模型，在面板的模型菜单里切换即可。
- **内置 DeepSeek 连接**：地址固定，只需填密钥，提供 DeepSeek-V4.1-Flash 与 DeepSeek-V4-Pro 两个模型；不能删除。不用 DeepSeek 时留空即可，不影响其他连接。
- **生效时机**：增删连接或连接测通后，面板在没有回合进行时自动重启运行时，新的模型与默认模型随即可用，正在打开的会话自动恢复；Agent 的默认模型和外部服务的请求头在新会话中生效。

### 1.1 检测

改动地址或密钥后自动检测，编辑器启动时也会补测填了密钥但还没检测过的连接；每条连接末尾的 Status 一行显示结论，旁边的刷新按钮可以手动重测：

| 结论 | 说明 |
|---|---|
| Connected | 已识别协议并列出模型；标题行显示协议与模型数 |
| Failed | 未连通；原因在 Output Log 里搜 `Connection test` |
| Testing... | 正在检测 |
| Untested | 还没有检测，通常是没填密钥 |

检测按 OpenAI 的方式取 `{地址}/models`，再依次试 `/responses` 与 `/chat/completions`；不是 OpenAI 的方式时按 Anthropic 取 `{地址}/v1/models`。不提供模型列表接口的服务暂不支持。每个请求的地址、状态码与耗时都写进 Output Log，不记录密钥。

## 2. 权限（个人）

这些开关管的是**运行时自己的能力**（读写文件、执行命令行）。编辑器里的改动（改属性、生成 Actor 等）不受它们影响，始终按会话的模式确认，见 [Tools.md §1](Tools.md#1-改动的确认与撤销)。

| 字段 | 默认值 | 说明 |
|---|---|---|
| Permissions > Default Write Mode | Manual | 新会话的模式：Manual 每次确认，Accept Edits 可撤销的改动直接执行，Auto 除删除、移动、改名外都直接执行 |
| Permissions > Allow Workspace Write | 关 | 打开后，运行时可以不经询问在项目目录内写文件 |
| Permissions > Allow Shell | 关 | 允许运行时使用 PowerShell |
| Permissions > Allow Elevation | 开 | 允许运行时申请超出沙箱的权限；关闭时这类申请直接拒绝 |
| Permissions > Permission Policy | Reject | 没有面板处理时，对运行时权限申请的默认回答 |

## 3. 工具开关（个人）

- **Editor Tools**：每个编辑器工具一个开关，关掉后 Agent 看不到也用不了，立即生效。
- **Engine Toolsets**：每个已启用的引擎 Toolset 一个开关；项目统一屏蔽的显示为锁定。

工具清单见 [Tools.md](Tools.md)。

## 4. 运行时与外观（个人）

| 字段 | 默认值 | 说明 |
|---|---|---|
| Runtime > Prewarm Runtime | 开 | 编辑器启动后提前启动运行时，打开面板时已经连好 |
| Runtime > Runtime Executable Override | 空 | 用其他运行时程序代替插件自带的，开发调试用 |
| Appearance > Font Size | 14 | 对话与输入框字号（11～20） |
| Appearance > Wide Layout | 关 | 对话铺满两侧窗格之间的宽度 |
| Appearance > Animate Mark | 开 | 播放标志动画 |
| Appearance > Ui Font Family | 空（Segoe UI、微软雅黑 UI） | 界面字体 |
| Appearance > Chat Font Family | 空（跟随界面字体） | 对话与输入框字体，可以填衬线字体如 Noto Serif SC |
| Appearance > Code Font Family | 空（Cascadia Mono、Consolas） | 代码、参数与日志的等宽字体 |

字体框点进去会列出本机已安装的字体，边输入边筛选，↑↓ 选择、回车确认；第一项“默认”清空设置。可以用逗号填多个，最多取前 4 个。填的字体本机没装时自动退回默认，框下方显示黄色提示。修改后面板立即生效。

## 5. 项目设置（团队）

由项目负责人维护，随项目进版本库，全团队生效。

| 字段 | 作用 | 详见 |
|---|---|---|
| Integration > Skill Dirs | Agent 可以按需加载的技能目录 | [Extending.md §2](Extending.md#2-技能) |
| Integration > Template Dirs | 斜杠模板目录 | [Extending.md §3](Extending.md#3-斜杠模板) |
| Integration > Ui Guides | Agent 可以帮用户打开的编辑器位置 | [Extending.md §4](Extending.md#4-界面指路) |
| Integration > External Mcp Servers | 接入的外部知识或工具服务 | [Extending.md §5](Extending.md#5-外部-mcp-服务) |
| Engine Toolsets > Blocked Toolset Names | 全团队屏蔽的引擎 Toolset 或工具 | [Tools.md §3](Tools.md#3-引擎-toolset) |
| Engine Toolsets > Read Only Tool Prefixes / Read Only Tools / Write Tools | 引擎工具哪些算只读、哪些算改动 | [Tools.md §3](Tools.md#3-引擎-toolset) |
| Engine Toolsets > Non Undoable Tools | 标为不可撤销的引擎工具 | [Tools.md §3.3](Tools.md#33-撤销) |
| Engine Toolsets > Destructive Tool Prefixes | 名字以这些词开头的引擎工具，确认卡标为删除、移动或改名 | [Tools.md §3](Tools.md#3-引擎-toolset) |

外部服务是否已接入、被跳过的原因，可以在面板检视窗格的 MCP 卡片里查看。

⚠️ 插件的默认名单写在插件的 `Config/DefaultShuttlegent.ini` 里。项目要增减条目时，在项目的 `Config/DefaultShuttlegent.ini` 里用 `+条目=` 追加、`-条目=` 删除，**不要整段重写**，否则插件的默认规则会被覆盖：

```ini
[/Script/ShuttlegentCore.ShuttlegentProjectSettings]
+ReadOnlyTools=SomeQueryTool
-BlockedToolsetNames=PluginToolset
```
