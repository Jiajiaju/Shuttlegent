# Shuttlegent

Shuttlegent 是一个 Unreal Editor 插件：在编辑器内提供 AI Agent 对话与工具调用，可接入常见 AI 供应商 API。插件与具体项目无关，项目特有的工具、提示词与知识来源通过扩展点注入。

名称取自织机上的梭子（shuttle）：梭子带着纬线在经线之间往返穿梭，Shuttlegent 在编辑器与 Agent 之间往返传递请求、工具调用与结果。

## 架构概览

- **Agent 底座**：[DeepSeek Harness](https://github.com/deepseek-ai/deepseek-harness)，作为编辑器的子进程运行，负责 Agent 循环、上下文管理、会话持久化与模型调用。
- **编辑器 ↔ Agent**：[ACP](https://github.com/agentclientprotocol/agent-client-protocol)（Agent Client Protocol），编辑器是 ACP 客户端，经 stdio 驱动 Agent。
- **Agent ↔ 编辑器工具**：MCP（Model Context Protocol），编辑器内开一个仅限本机的 MCP 端点，向 Agent 提供编辑器工具；写操作在编辑器侧逐次确认，可撤销。
- **模型供应商**：DeepSeek，以及 OpenAI、Anthropic 协议与任何 OpenAI 兼容端点（团队网关、本地模型服务）。

## 状态

早期开发中，目前只有插件骨架。

| 模块 | 类型 | 状态 |
|---|---|---|
| `ShuttlegentCore` | Editor | 空模块 |

## 环境要求

- Unreal Engine 5.8
- Windows

## 安装

1. 把本仓库放到工程的 `Plugins/Shuttlegent` 目录。
2. 在工程的 `.uproject` 中启用 `Shuttlegent` 插件（插件默认不启用）。
3. 重新生成工程文件并编译编辑器。

## 模型配置

模型供应商的端点 URL 和 API 密钥、外部 MCP 服务器请求头的密钥，都只从 **编辑器偏好设置 > Shuttlegent > General** 读取；不读 Windows 凭据管理器、环境变量或 `.env` 文件。这些设置保存在本机的 `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`，不进版本库；密钥以明文存放在这个文件里。

- **Providers > DeepSeek Base Url**：内置 DeepSeek 路由的端点（Anthropic Messages 协议），默认 `https://api.deepseek.com/anthropic`；留空也用这个地址。
- **Providers > DeepSeek Api Key**：内置 DeepSeek 路由的密钥；默认路由缺密钥时运行时不启动，面板提示到这里填写。
- **Providers > Providers**：其他路由（OpenAI、Anthropic 协议或 OpenAI 兼容端点），每个条目填自己的 Base Url 和 Api Key。新条目的 Base Url 默认是 DeepSeek 的 OpenAI 兼容端点 `https://api.deepseek.com`。
- **Providers > Default Provider / Default Model**：新会话使用的路由和模型；留空用内置 DeepSeek。
- **Credentials > Mcp Header Values**：变量名到值的映射，填项目设置（项目设置 > Shuttlegent > General）中外部 MCP 服务器请求头引用的变量（Value Variable）。

模型设置在重启运行时（面板中“重试”或重启编辑器）后生效；MCP 请求头的值在新会话中生效。

## 许可

待定。
