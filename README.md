# Shuttlegent

Shuttlegent 是一个 Unreal Editor 插件：在编辑器内提供 AI Agent 对话与工具调用，可接入常见 AI 供应商 API。插件与具体项目无关，项目特有的工具、提示词与知识来源通过扩展点注入。

名称取自织机上的梭子（shuttle）：梭子带着纬线在经线之间往返穿梭，Shuttlegent 在编辑器与 Agent 之间往返传递请求、工具调用与结果。

## 架构概览

- **Agent 底座**：[DeepSeek Harness](https://github.com/deepseek-ai/deepseek-harness)，作为编辑器的子进程运行，负责 Agent 循环、上下文管理、会话持久化与模型调用。
- **编辑器 ↔ Agent**：[ACP](https://github.com/agentclientprotocol/agent-client-protocol)（Agent Client Protocol），编辑器是 ACP 客户端，经 stdio 驱动 Agent。
- **Agent ↔ 编辑器工具**：MCP（Model Context Protocol），编辑器内开一个仅限本机的 MCP 端点，向 Agent 提供编辑器工具；写操作在编辑器侧逐次确认，可撤销。
- **模型供应商**：DeepSeek，以及 OpenAI、Anthropic 协议与任何 OpenAI 兼容端点（团队网关、本地模型服务）。

## 状态

版本 0.1.0，实验阶段，插件默认不启用。

已实现：

- **对话面板**：CEF Web 页面（`Resources/Web`），会话侧栏、对话列与检视窗格；Markdown 渲染、思考折叠、用量显示。
- **会话**：多会话并行、消息排队、取消、会话恢复；运行时故障时提示原因并可重试。
- **编辑器工具**：经本机 MCP 端点提供给 Agent；写操作在面板中逐次确认，执行后可撤销。
- **项目扩展**：项目提示词、技能、斜杠模板、外部 MCP 服务、界面指路、项目自定义工具。

| 模块 | 类型 | 职责 |
|---|---|---|
| `ShuttlegentCore` | Editor | 偏好设置与项目设置、生成运行时 patch、解析密钥、启动运行时 |
| `ShuttlegentMcpServer` | Editor | 本机 MCP 端点、写入门（确认与撤销）、外部 MCP 接入、工具注册接口 `IShuttlegentToolProvider` |
| `ShuttlegentTools` | Editor | 内置编辑器工具 |
| `ShuttlegentEditor` | Editor | 对话面板、ACP 会话、斜杠模板、设置页定制；`PostEngineInit` 加载 |
| `ShuttlegentToolsetBridge` | Editor | 模块骨架，尚无实现 |

## 内置工具

| 分组 | 工具 | 读写 |
|---|---|---|
| 关卡 | `list_actors`、`get_selection` | 读 |
| 资源 | `find_assets`、`get_asset`、`get_asset_references` | 读 |
| 反射 | `get_properties`、`describe_class`、`get_settings` | 读 |
| 诊断 | `read_log`、`get_cvar` | 读 |
| 界面指路 | `show_editor_ui` | 读 |
| 编辑 | `set_property`、`spawn_actor`、`delete_actors`、`save_assets` | 写 |
| 历史 | `undo`、`redo` | 写 |

写工具执行前在面板中弹出确认卡。每个工具可在 **编辑器偏好设置 > Shuttlegent > General** 的编辑器工具开关中单独关闭。

## 环境要求

- Unreal Engine 5.8
- Windows
- 构建运行时：Node.js 22.19+ 或 24+（启用 corepack）与 git，只在构建机上需要

## 安装

1. 把本仓库放到工程的 `Plugins/Shuttlegent` 目录，并拉取子模块：`git submodule update --init`。
2. 构建运行时：执行 `Build/BuildRuntime.ps1`，产物为 `Runtime/shuttlegent-runtime.exe`。
3. 在工程的 `.uproject` 中启用 `Shuttlegent` 插件（插件默认不启用）。
4. 重新生成工程文件并编译编辑器。
5. 在 **编辑器偏好设置 > Shuttlegent > General** 填写模型密钥（见下文“模型配置”）。

## 目录结构

| 目录 | 内容 |
|---|---|
| `Source/` | 插件 C++ 模块 |
| `Resources/Web/` | 对话面板页面（原生 JS） |
| `Resources/svg/` | 图标与标志 |
| `Config/` | 插件默认的项目设置 `DefaultShuttlegent.ini` |
| `Runtime/` | 运行时 profile patch；构建后的 `shuttlegent-runtime.exe` 也放在这里，不进版本库 |
| `Build/` | 运行时构建脚本、回归脚本（`Probe/`）、标志动画导出（`MarkAnim/`） |
| `Samples/` | 示例项目接入 |
| `ThirdParty/deepseek-harness/` | DeepSeek Harness 子模块 |

`Binaries/`、`Intermediate/`、`Saved/` 是生成内容，不进版本库。

## 模型配置

模型供应商的端点 URL 和 API 密钥、外部 MCP 服务器请求头的密钥，都只从 **编辑器偏好设置 > Shuttlegent > General** 读取；不读 Windows 凭据管理器、环境变量或 `.env` 文件。这些设置保存在本机的 `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`，不进版本库；密钥以明文存放在这个文件里。

- **Providers > DeepSeek Base Url**：内置 DeepSeek 路由的端点（Anthropic Messages 协议），默认 `https://api.deepseek.com/anthropic`；留空也用这个地址。
- **Providers > DeepSeek Api Key**：内置 DeepSeek 路由的密钥；默认路由缺密钥时运行时不启动，面板提示到这里填写。
- **Providers > Providers**：其他路由（OpenAI、Anthropic 协议或 OpenAI 兼容端点），每个条目填自己的 Base Url 和 Api Key。新条目的 Base Url 默认是 DeepSeek 的 OpenAI 兼容端点 `https://api.deepseek.com`。
- **Providers > Default Provider / Default Model**：新会话使用的路由和模型；留空用内置 DeepSeek。
- **Credentials > Mcp Header Values**：变量名到值的映射，填项目设置（项目设置 > Shuttlegent > General）中外部 MCP 服务器请求头引用的变量（Value Variable）。

模型设置在重启运行时（面板中“重试”或重启编辑器）后生效；MCP 请求头的值在新会话中生效。

## 扩展与示例

项目通过项目设置（**项目设置 > Shuttlegent > General**）和 `IShuttlegentToolProvider` 接入自己的提示词、技能、模板、知识服务与工具。`Samples/ShuttlegentSample` 覆盖全部扩展点，启用方法见 [Samples/ShuttlegentSample/README.md](Samples/ShuttlegentSample/README.md)。

## 回归测试

`Build/Probe/RunRegression.ps1` 不经编辑器直接驱动运行时，覆盖初始化、密钥、MCP 工具、只读沙箱、会话恢复、取消、网关路由与并行会话。升级 DeepSeek Harness 子模块或重新构建运行时后运行，详见 [Build/Probe/README.md](Build/Probe/README.md)。

## 许可

待定。
