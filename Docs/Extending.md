# 项目扩展

本文讲项目怎么接入自己的规则、流程、知识与工具，给项目负责人和技术美术看。

Shuttlegent 本身不绑定任何项目。项目可以把自己的规则、做事步骤、知识与工具接进来，让 Agent 更懂这个项目：

| 想做什么 | 用什么 | 在哪配置 | 要写代码吗 |
|---|---|---|---|
| 让 Agent 每次都遵守项目规则 | 项目提示词（§1） | 项目根目录 `SHUTTLEGENT.md` | 否 |
| 让 Agent 记住个人习惯 | 个人记忆（§1） | 对话里直接说“记住……” | 否 |
| 让 Agent 按固定步骤做某类事 | 技能（§2） | 项目设置 Integration > Skill Dirs | 否 |
| 把常用提问存起来，一键填入 | 斜杠模板（§3） | 项目设置 Integration > Template Dirs | 否 |
| 用户问“在哪里改”时直接打开对应位置 | 界面指路（§4） | 项目设置 Integration > Ui Guides | 否 |
| 接入项目知识库、术语表等服务 | 外部 MCP 服务（§5） | 项目设置 Integration > External Mcp Servers | 否（服务本身需另行提供） |
| 给 Agent 加项目专用的编辑器操作 | 自定义工具（§6） | C++ 模块 | 是 |

项目设置都保存在项目的 `Config/DefaultShuttlegent.ini`，随项目进版本库，全团队生效。`Samples/ShuttlegentSample` 是覆盖上面全部扩展点的示例，启用方法见 [Samples/ShuttlegentSample/README.md](../Samples/ShuttlegentSample/README.md)。

## 1. 提示词与记忆

- `SHUTTLEGENT.md`：团队共享的项目规则，放在项目根目录，提交到版本库。
- `SHUTTLEGENT.local.md`：个人补充，放在 `Saved/Shuttlegent/`，不进版本库。

每次对话都会带上这两个文件。Shuttlegent 不读 `AGENTS.md`、`CLAUDE.md`，以免和写代码用的规则混在一起。示例：

```markdown
# 项目说明

- 回答中提到关卡对象时，同时给出 Actor 的标签和类。
- 修改关卡前先看一眼关卡的组成。
```

个人偏好也可以直接让 Agent 记住（如“以后放灯光默认放在 /Game/Lights 下”）。记忆存在 `Saved/Shuttlegent/MEMORY.md`，每次请求都会带上，只属于你自己、不进版本库：

- Agent 记下或忘掉某条时，对话里显示一行提示，可以撤销；
- 每条一行（`- 内容（记录日期）`），可以在检视窗格的记忆卡片里删除，也可以直接编辑文件；
- 上限 8 KB，满了 Agent 会先合并或删掉旧条目；不记当前任务的细节、编辑器里能查到的东西和密钥。

## 2. 技能

技能是一套固定的做事步骤，Agent 判断需要时才加载。一个技能是一个目录，里面放一个 `SKILL.md`：开头的 `name` 和 `description` 说明它做什么、什么时候用，正文写步骤。

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

在 Skill Dirs 里添加技能所在的上级目录（相对项目目录），如 `Shuttlegent/Skills`。

### 2.1 插件自带的技能与模板

| 模板 | 做什么 |
|---|---|
| `/level-review` | 检查当前关卡：默认名称的 Actor、缺材质或用默认材质的网格体、光源数量、重复摆放；只汇报不修改 |
| `/batch-edit` | 把多个对象的同一属性改成同一个值，或按规则逐个改 |
| `/table-edit` | 改数据表前先导出备份；少量改动直接改，大量改动导出给你用 Excel 改再导回 |
| `/lighting-preset` | 把室外灯光调成正午、黄昏、阴天或夜晚 |
| `/material-edit` | 批量改材质实例参数、新建材质实例、给关卡里的物体换材质 |
| `/find-assets` | 按描述找资产，并查清被哪里用到；只查不改 |
| `/niagara-review` | 检查 Niagara 特效的编译错误与警告；你同意后逐条应用引擎给出的修复 |
| `/umg-review` | 检查控件蓝图里默认名称的控件、过深的层级、误设为变量的控件 |
| `/tag-review` | 找出没有资产引用、写法不统一、意思重复的 GameplayTag；删除与改名逐个确认 |

每个模板都有同名技能。不用模板、直接用自然语言描述同样的任务，Agent 也会加载对应技能。

- 插件自带的技能在插件的 `Resources/Skills/`，模板在 `Resources/Templates/`，排在项目目录之后加载。
- 项目里放一个同名的技能或模板，就会替换插件自带的那个。
- 不需要插件自带的技能与模板时，关闭 **项目设置 > Shuttlegent > General > Use Built-in Skills and Templates**。

## 3. 斜杠模板

模板是一个 `.md` 文件，在输入框行首输入 `/` 时出现在菜单里：

- 文件名就是 `/` 后面的名字；
- 第一行是菜单里显示的说明；
- 选中后整个文件填入输入框，可以修改后再发送。

Template Dirs 里的所有 `.md` 文件（包括子目录）都会读取；同名文件以先列出的目录为准。

```markdown
按关卡自查技能检查当前关卡，汇总成一张表：检查项、结果、建议。
```

## 4. 界面指路

每个条目描述一个编辑器位置。用户问“在哪里改某某”时，Agent 用 `show_editor_ui` 直接打开它。

| 字段 | 说明 |
|---|---|
| Id | Agent 使用的键，如 `world-settings` |
| Kind | `Tab`（编辑器页签）或 `Settings`（设置页） |
| Target | Tab 填页签 Id；Settings 填 `容器/分类/分节`，如 `Project/Engine/Rendering` |
| Description | 用户在那里能找到或做什么，Agent 据此判断该打开哪个 |

```ini
[/Script/ShuttlegentCore.ShuttlegentProjectSettings]
+UiGuides=(Id="world-settings",Kind=Tab,Target="WorldSettingsTab",Description="关卡的世界设置：GameMode、重力、Kill Z 等全关卡参数")
+UiGuides=(Id="rendering-settings",Kind=Settings,Target="Project/Engine/Rendering",Description="项目渲染设置：抗锯齿、全局光照、反射方法")
```

## 5. 外部 MCP 服务

外部 MCP 服务在每个会话开始时接入，它的工具名以服务名开头。

| 字段 | 说明 |
|---|---|
| Name | 服务名；不能用 `ue`，那是编辑器自己的名字 |
| Type | `Http` 或 `Stdio` |
| Url | Http：完整地址 |
| Headers | Http：请求头。密钥不写在项目配置里，只写变量名，由每个人在 **编辑器偏好设置 > Shuttlegent > General > Credentials > Mcp Header Values** 里填写 |
| Command / Arguments | Stdio：可执行文件的绝对路径与参数 |

配置不完整的条目会被跳过（缺名称、Http 缺地址、Stdio 命令不是绝对路径、请求头变量没有值），因为任何一个服务启动失败，整个会话都会开不起来。被跳过的条目和原因可以在面板检视窗格的 MCP 卡片里查看。

```ini
[/Script/ShuttlegentCore.ShuttlegentProjectSettings]
+ExternalMcpServers=(Name="glossary",Type=Stdio,Command="C:/UE/Engine/Binaries/ThirdParty/Python3/Win64/python.exe",Arguments=("-I","C:/Project/Tools/glossary_server.py"))
```

## 6. 自定义工具

需要 C++。项目或插件可以提供自己的编辑器工具，和内置工具走同一套确认与撤销，见 [Tools.md §1](Tools.md#1-改动的确认与撤销)。

1. 模块依赖 `ShuttlegentMcpServer`。
2. 实现 `IShuttlegentTool`：
   - 只读工具：实现 `Run`；
   - 改动工具：把定义中的 `bReadOnly` 设为 `false`，实现 `Preview`（列出要改的对象，不做修改）和 `Execute`（只改预览过的对象；在端点打开的事务里调用 `Modify()`，不要自己开事务）。
3. 实现 `IShuttlegentToolProvider::GetTools`，在模块启动时注册、关闭时注销：

```cpp
IModularFeatures::Get().RegisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider.Get());
```

工具名只能用小写字母、数字和下划线，不能和已有工具重名。完整示例见 `Samples/ShuttlegentSample/Source/ShuttlegentSample`。
