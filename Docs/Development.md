# 开发

本文面向修改 Shuttlegent 本身的开发者。架构见 [Architecture.md](Architecture.md)。

## 1. 仓库结构

| 目录 | 内容 |
|---|---|
| `Source/` | 插件的五个 C++ 模块 |
| `Resources/Web/` | 对话面板页面（原生 JS，无构建步骤） |
| `Resources/svg/`、`Resources/png/` | 图标与标志；标志、组合标志和插件图标由 `Build/Brand/` 生成，不手改 |
| `Config/` | 插件默认的项目设置 `DefaultShuttlegent.ini` |
| `Runtime/` | 运行时的 profile 补丁；构建出的 `shuttlegent-runtime.exe` 也放在这里，不进版本库 |
| `Build/` | 运行时构建脚本、回归脚本（`Probe/`）、标志生成（`Brand/`：唯一几何源 `brand-geometry.json`，运行 `ExportBrand.ps1` 重新生成全部标志 SVG/PNG 与面板动画用的 `Resources/Web/brand-geometry.js`） |
| `Samples/` | 示例项目接入 |
| `ThirdParty/deepseek-harness/` | DeepSeek Harness 子模块 |
| `Docs/` | 文档 |

`Binaries/`、`Intermediate/`、`Saved/` 是生成内容，不进版本库。

## 2. 构建运行时

只在构建机上需要：Node.js 22.19+ 或 24+（启用 corepack）与 git。pnpm 由 corepack 按子模块锁定的版本调用。

先拉取子模块：

```powershell
git submodule update --init
```

再构建运行时：

```powershell
& .\Build\BuildRuntime.ps1
```

脚本依次执行：
1. 安装依赖；
2. 构建 Harness；
3. 打包成单文件 exe（目前只支持 `node24-win-x64`）；
4. 把 `shuttlegent-runtime.exe`、ripgrep 和资源目录复制到 `Runtime/`；
5. 写入 Shuttlegent 版本信息。

依赖已安装、已构建时，可以分别加 `-SkipInstall`、`-SkipBuild` 跳过对应步骤。

## 3. 编译插件

插件随项目一起编译。在引擎根目录执行，`<Project>.uproject` 换成项目文件的绝对路径：

```powershell
& .\Engine\Build\BatchFiles\Build.bat <Project>Editor Win64 Development -Project=<Project>.uproject -WaitMutex -NoHotReloadFromIDE
```

编辑器开着 Live Coding 时无法编译，会报 “Unable to build while Live Coding is active”，先关闭编辑器。

## 4. 回归测试

`Build/Probe/RunRegression.ps1` 不经编辑器，直接驱动运行时，对一个模拟的编辑器 MCP 端点运行场景。覆盖：
- 初始化、缺少密钥、密钥错误；
- MCP 读写工具、只读沙箱；
- 会话恢复、取消；
- 网关路由、并行会话。

升级 Harness 子模块或重新构建运行时后都要跑一遍。

不调用模型（不需要密钥）：

```powershell
& .\Build\Probe\RunRegression.ps1 -SkipLiveModel
```

完整回归（会消耗模型 token；密钥取自项目的编辑器偏好设置）：

```powershell
& .\Build\Probe\RunRegression.ps1
```

场景清单与参数见 [Build/Probe/README.md](../Build/Probe/README.md)。

## 5. 控制台命令

| 命令 | 作用 |
|---|---|
| `Shuttlegent.Start` / `Shuttlegent.Stop` | 启动 / 停止运行时 |
| `Shuttlegent.New` / `Shuttlegent.Resume` / `Shuttlegent.List` | 新建、恢复、列出会话 |
| `Shuttlegent.Prompt` / `Shuttlegent.Cancel` | 发送提问 / 取消回合，对话打印到 Output Log |
| `Shuttlegent.Model` | 切换模型 |
| `Shuttlegent.WriteProbe` | 经写入门把选中的 Actor 上移，用于检查确认卡与撤销 |

日志分类：`LogShuttlegent`（插件）、`LogToolsetRegistry`（引擎 Toolset）。

## 6. 排查备忘

| 现象 | 原因 | 解法 |
|---|---|---|
| 编译报 “Unable to build while Live Coding is active” | 编辑器的 Live Coding 占用了编辑器 Target | 关闭编辑器后再编译 |
| 打包后的运行时加载原生模块 `koffi` 失败 | 启动目录不在系统盘 | `DSH_HOME` 和启动目录放系统盘（默认已是） |
| 回归脚本启动运行时报 WinError 267 | 用了 Python Install Manager 安装的 Python | 使用引擎自带的 Python，或传 `-Python` |
| 覆盖 Harness 补丁的某一行后，其他配置字段丢失 | 补丁覆盖时整体替换 `config`，不合并 | 覆盖时写全该行的 `config` |
| 项目设置里的读写规则或屏蔽名单失效 | 项目配置整段重写了数组 | 用 `+条目=` 追加、`-条目=` 删除 |
