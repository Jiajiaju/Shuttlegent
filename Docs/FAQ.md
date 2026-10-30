# 常见问题

## 1. 面板提示“缺少密钥”或“鉴权失败”

- **缺少密钥**：在 **编辑器偏好设置 > Shuttlegent > General > Providers** 填写 DeepSeek Api Key；使用其他供应商时，填写对应条目的 Api Key。
- **鉴权失败**：检查密钥是否正确、是否过期，以及 Base Url 是否和密钥属于同一供应商。

改完后在面板点“重试”。

## 2. 面板提示找不到运行时

插件目录下缺少 `Runtime/shuttlegent-runtime.exe`。按 [Development.md §2](Development.md#2-构建运行时) 构建运行时，或者在偏好设置的 Runtime Executable Override 中指定一个已有的运行时。

## 3. 确认卡一直不出现，写操作被拒绝

写操作需要面板打开才能确认；面板关闭时，写请求会被直接拒绝。确认卡出现后 55 秒内未确认，也视为放弃。请保持面板打开，再让 Agent 重试。

## 4. Agent 说没有某个引擎 Toolset

依次检查三处：

1. 该 Toolset 插件是否已在 **编辑 > 插件** 中启用，并且已重启编辑器；
2. **项目设置 > Shuttlegent > General > Engine Toolsets > Blocked Toolset Names** 是否屏蔽了它；
3. **编辑器偏好设置 > Shuttlegent > General > Engine Toolsets** 中，它的开关是否打开。

## 5. 改动能撤销吗

- 内置写工具在编辑器事务中执行，可以在面板结果栏撤销，也可以用 Ctrl+Z。
- 保存资源、新建资源这类操作无法从编辑器撤销，确认卡会标出“不可撤销”。
- 引擎 Toolset 工具能否撤销，以执行结果为准。

详见 [Tools.md §1](Tools.md#1-写操作确认与撤销)。

## 6. 密钥存在哪里，会不会进版本库

密钥只保存在本机的 `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`，以明文存放，不进版本库，也不会写进运行时的补丁文件。运行时通过子进程环境变量拿到密钥。

## 7. 改了模型设置没生效

模型、供应商和权限开关在运行时重启后才生效：在面板点“重试”，或重启编辑器。工具开关和 Toolset 开关立即生效。

## 8. 能不能用其他模型

可以。在 **编辑器偏好设置 > Providers > Providers** 添加条目，支持：

- OpenAI 协议；
- Anthropic 协议；
- 任何 OpenAI 兼容端点，如团队网关、本地模型服务。

然后在 Default Provider / Default Model 中选择这个条目，或在面板的模型下拉框中切换。
