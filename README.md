<p align="center">
  <img src="Resources/png/lockup-horizontal-color.png" alt="Shuttlegent" width="420">
</p>

<p align="center"><b>面向策划与美术的 Unreal Editor 内 AI 助手</b></p>

<p align="center">
  <img src="https://img.shields.io/badge/Unreal%20Engine-5.8-313131" alt="Unreal Engine 5.8">
  <img src="https://img.shields.io/badge/platform-Windows-0078D6" alt="Windows">
  <img src="https://img.shields.io/badge/version-0.1.0%20experimental-orange" alt="0.1.0 experimental">
  <img src="https://img.shields.io/badge/license-MIT-green" alt="MIT">
</p>

<!-- 面板截图待补：Docs/Images/panel.png -->

在编辑器里用一句话完成关卡、Actor、资源和设置上的操作：查、改、摆放、整理。Agent 要做的每一处改动都先给你看，确认后才执行，执行后可以撤销。

- **就在编辑器里**：面板停靠在编辑器中，可以直接引用选中的 Actor，也可以从内容浏览器把资源拖进对话。
- **改动由你确认**：每次写操作都先弹出确认卡，列出对象、字段、当前值和计划值；确认后在编辑器事务中执行，可以撤销。
- **模型自己选**：默认使用 DeepSeek，也可以接 OpenAI、Anthropic 协议，或团队网关、本地模型服务。
- **用上引擎自带的 AI 工具**：接入 UE 5.8 的 Toolset Registry，UMG、Niagara、PCG、GameplayTags、StateTree 等系统的工具都能用。

名称取自织机上的梭子（shuttle）：梭子带着纬线在经线之间往返穿梭，Shuttlegent 在编辑器与 Agent 之间往返传递请求、工具调用与结果。

## 它能做什么

| 你说 | Shuttlegent 做 |
|---|---|
| “把选中的这几盏灯的强度都调到 5000” | 读出每盏灯的当前强度，弹出确认卡列出改前和改后，确认后一次改完 |
| “这个关卡里还有哪些 Actor 用着默认名字？” | 列出标签仍是默认名的 Actor，不做任何修改 |
| “哪些地方引用了 Hero 这个角色资源？” | 查出引用它的资源和关卡 |
| “在玩家出生点旁边放一盏点光源” | 找到出生点，生成点光源，执行前先确认 |
| “项目的抗锯齿方法在哪里改？” | 告诉你当前设置，并直接打开对应的设置页 |
| “给这个界面加一个确认按钮” | 通过 UMG Toolset 修改控件蓝图，执行前先确认（需启用 UMGToolSet） |
| “刚才那步改错了，撤销” | 列出要撤销的步骤，确认后撤销 |

## 快速开始

### 1. 环境要求

- Unreal Engine 5.8，Windows
- 一个模型 API 密钥，默认使用 [DeepSeek](https://platform.deepseek.com/)

### 2. 安装插件

1. 把本仓库放到项目的 `Plugins/Shuttlegent` 目录。
2. 准备运行时 `Runtime/shuttlegent-runtime.exe`。预编译版本尚未发布，目前需要自行构建，见 [Docs/Development.md §2](Docs/Development.md#2-构建运行时)。
3. 在项目的 `.uproject` 中启用 `Shuttlegent` 插件（插件默认不启用），重新生成工程文件并编译编辑器。

### 3. 填写密钥

打开 **编辑器偏好设置 > Shuttlegent > General**，在 Providers 中填写 DeepSeek Api Key。其他供应商的配置方法见 [Docs/Configuration.md](Docs/Configuration.md)。

### 4. 开始对话

从 **窗口 > Shuttlegent** 打开面板，输入你想在编辑器里完成的事，按 Enter 发送。

- 输入 `@`、按 Ctrl+Space，或从内容浏览器、大纲视图拖入，可以引用 Actor 和资源。
- 输入 `/` 可以选择项目提供的模板。
- 回合进行中继续发送的消息会排队，随时可以停止当前回合。

### 5. 启用引擎 Toolset（可选）

在 **编辑 > 插件** 中搜索 Toolset，启用需要的系统，比如 EditorToolset、UMGToolSet、NiagaraToolsets、PCGToolset，然后重启编辑器。推荐清单见 [Docs/Tools.md §3.1](Docs/Tools.md#31-启用-toolset)。

## 你掌握控制权

- **写操作逐次确认**：面板未打开时，写请求直接拒绝；确认卡 55 秒内未确认视为放弃。
- **可以撤销**：每次写操作都在编辑器事务中执行，任一对象失败时整批回滚；不能撤销的操作（如保存）会在确认卡上标明。
- **运行时默认只读**：Agent 的文件与命令行能力默认关闭或受限，只能通过编辑器工具改动项目。
- **默认屏蔽高风险能力**：写代码、开关插件、模拟点击界面等引擎 Toolset 默认屏蔽；每个工具和 Toolset 都可以单独关闭。
- **密钥只存本机**：密钥只保存在本机的编辑器偏好设置中，不进版本库，也不写入任何补丁文件。

## 功能

| 方面 | 内容 |
|---|---|
| 对话面板 | 多会话并行、会话搜索与归档、消息排队、思考过程折叠、用量显示、模型与思考强度切换 |
| 内置工具 | 关卡、资源、反射、诊断、编辑、撤销重做、界面指路，共 17 个，见 [Docs/Tools.md §2](Docs/Tools.md#2-内置工具) |
| 引擎 Toolset | 通过 `list_toolsets`、`describe_toolset`、`call_toolset_tool` 使用已启用的 Toolset，见 [Docs/Tools.md §3](Docs/Tools.md#3-引擎-toolset) |
| 模型 | DeepSeek（默认）、OpenAI 协议、Anthropic 协议、任何 OpenAI 兼容端点 |
| 项目扩展 | 项目提示词、技能、斜杠模板、界面指路、外部 MCP 服务、自定义工具，见 [Docs/Extending.md](Docs/Extending.md) |

## 文档

| 文档 | 内容 |
|---|---|
| [Configuration.md](Docs/Configuration.md) | 模型与密钥、权限、工具开关、项目设置 |
| [Tools.md](Docs/Tools.md) | 写操作确认与撤销、内置工具、引擎 Toolset |
| [Extending.md](Docs/Extending.md) | 项目提示词、技能、模板、界面指路、外部 MCP、自定义工具 |
| [Architecture.md](Docs/Architecture.md) | 模块组成、运行时与 ACP、MCP 端点、对话面板 |
| [Development.md](Docs/Development.md) | 仓库结构、构建运行时、编译、回归测试、排查 |
| [FAQ.md](Docs/FAQ.md) | 常见问题 |

## 架构一览

Shuttlegent 由编辑器内的五个模块和一个运行时子进程组成：

- **Agent 底座**：[DeepSeek Harness](https://github.com/deepseek-ai/deepseek-harness)，以子进程方式运行，负责 Agent 循环、上下文管理与模型调用。
- **编辑器 ↔ Agent**：编辑器通过 [ACP](https://github.com/agentclientprotocol/agent-client-protocol)（Agent Client Protocol）驱动 Agent。
- **Agent ↔ 编辑器工具**：Agent 通过 MCP 调用编辑器内只对本机开放的端点；写操作先回到编辑器确认。

详见 [Docs/Architecture.md](Docs/Architecture.md)。

## 许可

[MIT](LICENSE)。子模块 `ThirdParty/deepseek-harness` 遵循其自身的许可。

## 致谢

- [DeepSeek Harness](https://github.com/deepseek-ai/deepseek-harness)：Agent 运行时
- [Agent Client Protocol](https://github.com/agentclientprotocol/agent-client-protocol)：编辑器与 Agent 之间的协议
- [Model Context Protocol](https://modelcontextprotocol.io/)：Agent 调用编辑器工具的协议
