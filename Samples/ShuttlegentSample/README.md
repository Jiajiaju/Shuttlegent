# Shuttlegent Sample

Shuttlegent 的示例项目接入，覆盖 Shuttlegent.md 第 11 节的全部扩展点：

| 扩展点 | 示例 |
|---|---|
| 工具 | `Source/ShuttlegentSample`：经 `IShuttlegentToolProvider` 注册 `sample_count_actors`（读）与 `sample_set_actor_label`（写，经确认卡、可撤销） |
| 外部知识服务 | `Mcp/sample_glossary_server.py`：stdio MCP 词汇表服务，工具名 `lookup` |
| 项目提示词 | `Project/SHUTTLEGENT.md` |
| 技能 | `Project/Shuttlegent/Skills/level-review` |
| 斜杠模板 | `Project/Shuttlegent/Templates/*.md` |
| 界面指路 | 下方配置中的 `UiGuides` |
| 权限开关 | 编辑器偏好设置 → 插件 → Shuttlegent：编辑器工具开关中可关掉示例工具 |

## 启用

UE 在一个插件目录里找到 `.uplugin` 后不再向下查找，所以本示例放在 Shuttlegent 插件目录内时不会被加载。启用时把它链接到项目的 `Plugins/` 下（目录链接不进版本库）：

```
mklink /J "<ProjectDir>\Plugins\ShuttlegentSample" "<ShuttlegentPluginDir>\Samples\ShuttlegentSample"
```

再把 `Project/` 下的文件复制到项目根目录，并把下面的配置追加到项目的 `Config/DefaultShuttlegent.ini`（`<EnginePython>` 换成引擎自带 Python 的绝对路径，`<ShuttlegentPluginDir>` 换成 Shuttlegent 插件目录的绝对路径，路径用 `/`）：

```ini
[/Script/ShuttlegentCore.ShuttlegentProjectSettings]
+SkillDirs=(Path="Shuttlegent/Skills")
+TemplateDirs=(Path="Shuttlegent/Templates")
+UiGuides=(Id="world-settings",Kind=Tab,Target="WorldSettingsTab",Description="关卡的世界设置：GameMode、重力、Kill Z 等全关卡参数")
+UiGuides=(Id="rendering-settings",Kind=Settings,Target="Project/Engine/Rendering",Description="项目渲染设置：抗锯齿、全局光照、反射方法")
+ExternalMcpServers=(Name="glossary",Type=Stdio,Command="<EnginePython>",Arguments=("-I","<ShuttlegentPluginDir>/Samples/ShuttlegentSample/Mcp/sample_glossary_server.py"))
```

重新编译并启动编辑器后，示例工具出现在工具列表里；配置在下次启动运行时生效。

## 停用

删除目录链接，删去复制到项目的文件与追加的配置，重新编译。
