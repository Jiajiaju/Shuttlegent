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

## 许可

待定。
