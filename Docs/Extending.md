# 项目扩展

Shuttlegent 与具体项目无关，项目通过以下扩展点接入自己的规则、流程、知识与工具。除提示词文件和自定义工具外，都在 **项目设置 > Shuttlegent > General** 中配置，保存到项目的 `Config/DefaultShuttlegent.ini`，随项目进版本库。

`Samples/ShuttlegentSample` 覆盖下面全部扩展点，启用方法见 [Samples/ShuttlegentSample/README.md](../Samples/ShuttlegentSample/README.md)。

| 扩展点 | 用途 | 配置位置 |
|---|---|---|
| 项目提示词 | 项目规则与约定，每次对话都带上 | 项目根目录 `SHUTTLEGENT.md`、`SHUTTLEGENT.local.md` |
| 技能 | 一套固定的做事步骤，Agent 按需加载 | Integration > Skill Dirs |
| 斜杠模板 | 常用提问，输入 `/` 选择后填入输入框 | Integration > Template Dirs |
| 界面指路 | Agent 可以帮用户打开的编辑器位置 | Integration > Ui Guides |
| 外部 MCP 服务 | 项目知识库、术语表等外部服务 | Integration > External Mcp Servers |
| 自定义工具 | 项目专用的编辑器工具 | C++：实现 `IShuttlegentToolProvider` |

## 1. 项目提示词

- `SHUTTLEGENT.md`：团队共享的项目规则，放项目根目录，提交到版本库。
- `SHUTTLEGENT.local.md`：个人补充，放项目根目录，不要提交。

Shuttlegent 只读这两个文件，不读 `AGENTS.md`、`CLAUDE.md`，以免与写代码用的规则混在一起。示例：

```markdown
# 项目说明

- 回答中提到关卡对象时，同时给出 Actor 的标签和类。
- 修改关卡前先看一眼关卡的组成。
```

## 2. 技能

技能是一个目录，内含 `SKILL.md`。文件开头的 `name` 和 `description` 告诉 Agent 这个技能做什么、什么时候用；正文写步骤。Agent 只在需要时加载正文。

```markdown
---
name: level-review
description: 按清单检查当前关卡：光源数量、未命名 Actor、隐藏的 Actor。用户要求检查关卡时使用。
---

# 关卡自查

1. 统计各类 Actor 数量，光源超过 20 个时提醒。
2. 找出标签仍是默认名的 Actor，列出前 10 个。
3. 只汇报，不修改。
```

在 Skill Dirs 中添加技能所在的上级目录（相对项目目录），如 `Shuttlegent/Skills`。

## 3. 斜杠模板

模板是一个 `.md` 文件：

- 文件名就是 `/` 后输入的名字；
- 第一行是菜单里显示的说明；
- 整个文件会填入输入框，用户可以修改后再发送。

Shuttlegent 会读取 Template Dirs 中的所有 `.md` 文件，包括子目录。同名文件以先列出的目录为准。

```markdown
按关卡自查技能检查当前关卡，汇总成一张表：检查项、结果、建议。
```

## 4. 界面指路

每个条目描述一个编辑器位置。用户问“在哪里改某某”时，Agent 用 `show_editor_ui` 为用户打开该位置。

| 字段 | 说明 |
|---|---|
| Id | Agent 使用的键，如 `world-settings` |
| Kind | `Tab`（编辑器页签）或 `Settings`（设置页） |
| Target | Tab：页签 Id；Settings：`容器/分类/分节`，如 `Project/Engine/Rendering` |
| Description | 用户在那里能找到或做什么，会告诉 Agent |

```ini
[/Script/ShuttlegentCore.ShuttlegentProjectSettings]
+UiGuides=(Id="world-settings",Kind=Tab,Target="WorldSettingsTab",Description="关卡的世界设置：GameMode、重力、Kill Z 等全关卡参数")
+UiGuides=(Id="rendering-settings",Kind=Settings,Target="Project/Engine/Rendering",Description="项目渲染设置：抗锯齿、全局光照、反射方法")
```

## 5. 外部 MCP 服务

外部 MCP 服务在每个会话开始时声明给运行时，其工具名以服务名开头。

| 字段 | 说明 |
|---|---|
| Name | 服务名；不能用 `ue`，那是编辑器端点的名字 |
| Type | `Http` 或 `Stdio` |
| Url | Http：绝对 URL |
| Headers | Http：请求头。值不写在项目配置里，而是写变量名，由每个人在 **编辑器偏好设置 > Credentials > Mcp Header Values** 中填写 |
| Command / Arguments | Stdio：可执行文件的绝对路径与参数 |

配置不完整的条目会被跳过，并在 Output Log 中警告：缺名称、Http 缺 URL、Stdio 命令不是绝对路径、请求头变量没有值。这是因为任一服务启动失败，运行时都会拒绝整个会话。

```ini
[/Script/ShuttlegentCore.ShuttlegentProjectSettings]
+ExternalMcpServers=(Name="glossary",Type=Stdio,Command="C:/UE/Engine/Binaries/ThirdParty/Python3/Win64/python.exe",Arguments=("-I","C:/Project/Tools/glossary_server.py"))
```

## 6. 自定义工具

项目或插件可以用 C++ 提供自己的编辑器工具。工具与内置工具走同一套确认与撤销，见 [Tools.md §1](Tools.md#1-写操作确认与撤销)。

1. 模块依赖 `ShuttlegentMcpServer`。
2. 实现 `IShuttlegentTool`：
   - 只读工具：实现 `Run`；
   - 写工具：把定义中的 `bReadOnly` 设为 `false`，实现 `Preview`（列出要改的对象，不做修改）和 `Execute`（只改预览过的对象；在端点打开的事务里调用 `Modify()`，不要自己开事务）。
3. 实现 `IShuttlegentToolProvider::GetTools`，在模块启动时注册，关闭时注销：

```cpp
IModularFeatures::Get().RegisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider.Get());
```

工具名只能用小写字母、数字和下划线，且不能和已有工具重名。完整示例见 `Samples/ShuttlegentSample/Source/ShuttlegentSample`。
