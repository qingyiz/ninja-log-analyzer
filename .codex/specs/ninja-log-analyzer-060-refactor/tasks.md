# 实施计划：Ninja 日志分析器 0.6.0 架构重构

> 阶段：tasks
>
> 状态：待校验
>
> 最近更新：2026-07-23

## 执行策略

- 所有 5 项均为 required；按依赖顺序执行，每项实现与直接测试同批完成。
- 不重写 core 算法；若回归暴露 core 缺陷，先重开 design/requirements，不在重构中顺手扩张。
- 每项只有一个主要模块/构建单元；跨模块仅通过设计中公开契约连接。
- macOS deployment 只在本机原生验证；Windows/Linux 明确为未验证。

## 执行波次

| 波次 | 任务 | 完成后可观察状态 |
|---|---|---|
| 1 | TASK-001 | 文件分析 pipeline 可由无 Widgets service 独立调用和测试 |
| 2 | TASK-002 | 主窗口只协调状态，三个结果页独立渲染且交互保持 |
| 3 | TASK-003 | CMake target/目录依赖与架构一致，GUI 源码只编译一次 |
| 4 | TASK-004 | build/install/deploy bundle 契约和文档可执行 |
| 5 | TASK-005 | 双 Qt、结构、Spec 和原生交付证据闭环 |

## 任务列表

- [x] TASK-001：抽取无 Widgets 的分析加载用例
  - 类型：required
  - 需求：REQ-001；NFR-001、NFR-005
  - 设计：DEC-001 / AnalysisService / ARCH-001、ARCH-003 / PROP-001
  - 单一变更原因：把文件定位和完整分析 pipeline 从窗口状态中移到可独立测试的 application 用例。
  - 模块/构建单元：`ninja_analyzer_application`（初始可由现有顶层临时声明，TASK-003 下沉）。
  - 架构约束：遵守 ARCH-001/003、BUILD-002；application 只依赖 core/Qt Core，不依赖 Widgets。
  - 依赖变化：新增 `application -> core`、`application -> Qt Core`；移除后续 `MainWindow -> parser/manifest/analyzer` 直接 include。
  - 平台/交付物：平台无关，不产生最终交付物。
  - 依赖：无
  - 修改范围：新增 `src/application/AnalysisService.{h,cpp}`、`tests/tst_application.cpp`；临时更新构建清单；不改 core 算法和 GUI 页面。
  - 产出：`locateLogs`、`loadLog`、`LoadedAnalysis`，覆盖成功、错误、manifest/批次与 demo 基线的测试。
  - 验证：构建 `ninja_analyzer_application_tests`；`ctest -R ninja_analyzer_application_tests --output-on-failure`；现有 core tests。
  - 实施记录：新增 `src/application/AnalysisService.{h,cpp}` 与 `tests/tst_application.cpp`；service 统一定位、加载、分类、批次与视图分析契约。Qt5/Qt6 application/core 测试通过；demo 验证 19 条记录、2 批次、最后批次 14 条，覆盖 PROP-001。

- [x] TASK-002：拆分结果页面并瘦身窗口壳
  - 类型：required
  - 需求：REQ-002；NFR-002、NFR-003、NFR-004
  - 设计：DEC-001 / OverviewPage、SlowTasksPage、TimelinePage、AnalysisResultsWidget / ARCH-001—003 / PROP-002、PROP-003
  - 单一变更原因：让每个结果页独立拥有 UI/渲染职责，并使 MainWindow 只协调输入、批次、过滤和原子状态。
  - 模块/构建单元：`ninja_analyzer_gui`。
  - 架构约束：遵守 ARCH-001/002/003、BUILD-002；页面不访问文件，页面之间只通过 results widget/signals 协作。
  - 依赖变化：`gui -> application`；MainWindow 移除对 `LogLocator/NinjaLogParser/NinjaManifestParser/BuildAnalyzer` 的直接 include；无反向依赖。
  - 平台/交付物：平台无关 UI 源码；不单独产生最终交付物。
  - 依赖：TASK-001。
  - 修改范围：新增结果页面、results widget、`AppStyle`；重写 `MainWindow.*`；更新 `tst_mainwindow.cpp`；不改变 `OverviewChartsWidget`、`TimelineWidget` 算法和 core。
  - 产出：保持对象名和交互的三个页面；MainWindow 通过 AnalysisService 加载；生产 `.cpp` 职责预算满足。
  - 验证：offscreen `ninja_analyzer_gui_tests` 覆盖加载、批次、过滤、下钻、失败保持；运行 `inspect_structure.py`。
  - 实施记录：新增 `AnalysisResultsWidget`、Overview/SlowTasks/Timeline 页面和 `AppStyle`，MainWindow 改用 service 并从 903 行降至 359 行；保留测试 objectName、批次/过滤/下钻和失败原子性。Qt5/Qt6 GUI CTest 通过，覆盖 PROP-002/003。

- [x] TASK-003：将 CMake 清单重构为架构 target 图
  - 类型：required
  - 需求：REQ-003；NFR-003、NFR-004
  - 设计：DEC-002 / BUILD-001、BUILD-002 / ARCH-001、ARCH-002 / PROP-004
  - 单一变更原因：让源码所有权、依赖传递和测试所有权在就近 CMake target 中显式表达。
  - 模块/构建单元：CMake workspace 编排与各目录 targets。
  - 架构约束：遵守 ARCH-001/002、BUILD-001/002；顶层不列业务源码或测试 target；禁止全局 include/link 定义。
  - 依赖变化：app 改链 gui；gui 改链 application/core；application 链 core；GUI test 由重复源码改为链接 gui。
  - 平台/交付物：所有平台的 build graph；本任务不收集运行时、不产生部署产物。
  - 依赖：TASK-001、TASK-002。
  - 修改范围：根 `CMakeLists.txt`、`src/*/CMakeLists.txt`、`tests/CMakeLists.txt`、兼容 module；不改业务行为。
  - 产出：顶层 <=45 行；core/application/gui/app/tests 就近声明；AGL guard 在单责 module；输出根为 `<build>/bin`。
  - 验证：Qt6/Qt5 分别 configure；独立 build 四个生产 targets 和 test targets；CTest；`inspect_structure.py` 不再报顶层多职责。
  - 实施记录：根 CMake 降为 32 行编排；新增 core/application/gui/app/tests 就近清单和 Qt compatibility module；composition 移至 `src/app`；GUI test 仅链接生产 `ninja_analyzer_gui`。Qt5/Qt6 七个 targets 独立构建、三项 CTest 通过，inspect 不再报告顶层多职责，覆盖 PROP-004。

- [x] TASK-004：建立 macOS 应用安装与自包含部署规则
  - 类型：required
  - 需求：REQ-004；NFR-002、NFR-004
  - 设计：DEC-003 / BUILD-003 / macOS 应用束约束 / PROP-005
  - 单一变更原因：把“可编译 `.app`”提升为路径、元数据和运行时依赖均可验证的部署 bundle。
  - 模块/构建单元：`ninja_log_analyzer` + `cmake/NinjaAnalyzerDelivery.cmake`。
  - 架构约束：遵守 BUILD-003；部署/安装细节不得进入 core/application/gui 清单；使用 active Qt Kit 的工具。
  - 依赖变化：无生产源码 include/link 变化；install 阶段新增对 active `macdeployqt` 可执行文件的工具依赖。
  - 平台/交付物：macOS arm64 开发 `<build>/bin/Ninja Log Analyzer.app`；部署 `<stage>/Ninja Log Analyzer.app`；Windows/Linux 仅通用 runtime install 契约且未验证。
  - 依赖：TASK-003。
  - 修改范围：delivery CMake module、app bundle plist/template、`README.md`；不增加 DMG、签名、公证。
  - 产出：0.2.0 bundle 元数据、macOS 11.0 target、install-time Qt runtime 收集、精确使用/验证文档。
  - 验证：Qt6 和 Qt5 build bundle 结构；两套 Kit install；Qt6 `verify_delivery.py --require-self-contained`；`file`/`plutil`/`otool`；部署应用加载 demo 的启动 smoke。
  - 实施记录：项目升至 0.2.0；新增 plist、delivery module 和 install-time active-Kit `macdeployqt`，默认 macOS target 11.0。Qt6 首次结构检查虽通过但启动暴露缺少 `LC_RPATH`，随后在部署脚本中按检测补 `@executable_path/../Frameworks`。新 stage 的 Qt6/Qt5 bundle 均通过 `--require-self-contained`，含 cocoa plugin；Qt6 arm64/minos 11.0/版本 0.2.0，cocoa 实际启动加载 demo 后保持运行，覆盖 PROP-005。

- [x] TASK-005：闭环规格与跨层验收证据
  - 类型：required
  - 需求：REQ-001、REQ-002、REQ-003、REQ-004；NFR-001—005
  - 设计：全部 PROP-001—005 / 测试策略 / 需求覆盖矩阵
  - 单一变更原因：把各任务证据汇总为可复核的代码—规格—平台交付闭环并关闭 Spec。
  - 模块/构建单元：仓库验证与 `.codex/specs/ninja-log-analyzer-060-refactor`。
  - 架构约束：审计 ARCH-001—003、BUILD-001—003；发现漂移必须重开上游而不是修改结论。
  - 依赖变化：无。
  - 平台/交付物：验证 TASK-004 的 macOS arm64 两层 `.app`；Windows/Linux 明确未验证。
  - 依赖：TASK-004。
  - 修改范围：本 Spec 的实施记录/覆盖表、根 `AGENTS.md` 受管入口；仅在证据需要时修正文档，不新增功能。
  - 产出：每条 AC/PROP/平台产物有命令与结果；所有 required 任务完成；Spec 状态 complete。
  - 验证：全新 Qt6/Qt5 build+CTest；Release 10 万测试；inspect/validate/status；`verify_delivery.py`；git diff/status 审计。
  - 实施记录：Qt6 Release CTest 3/3（最终 1.24s）、Qt5 Release CTest 3/3（最终 0.87s）；Qt6 两个 10 万记录专项共 232ms；Qt6 严格 `-Wall -Wextra -Wpedantic` 构建与 CTest 通过且无项目警告。target graph 证实 app→gui→application→core、gui→core PRIVATE、tests→被测层，无环且 GUI 源码不重复编译。最终 inspect 显示 MainWindow 359 行、顶层 CMake 38 行且不再触发两项生产复杂度问题；旧 `tst_core.cpp` 521 行提示不违反生产文件预算。Qt6/Qt5 部署 bundle 自包含检查与 cocoa 启动通过；Windows/Linux 明确未验证。`git diff --check`、0.6.0 validate 和 AGENTS sync 通过。

## 覆盖检查

| 行为 | 实现任务 | 验证任务/证据 | 状态 |
|---|---|---|---|
| REQ-001 | TASK-001 | application/core tests、demo 19/2/14 基线 | 已验证 |
| REQ-002 | TASK-002 | Qt5/Qt6 offscreen GUI tests、双 Kit 启动 | 已验证 |
| REQ-003 | TASK-003 | target graph、独立 targets、inspect、严格警告 build | 已验证 |
| REQ-004 | TASK-004 | 双 Qt build/install、verify_delivery、自包含和 cocoa smoke | 已验证（macOS arm64） |

## 完成门槛

- [x] 所有 required 任务完成。
- [x] REQ-001—004 与 PROP-001—005 均有验证证据。
- [x] Qt6/Qt5 全新构建与全部 CTest 通过，性能门槛保持。
- [x] macOS 开发/部署 bundle 的路径、结构、依赖、架构和启动均有原生证据。
- [x] 0.6.0 inspect/validate、架构依赖与代码—规格一致性审计通过。
- [x] Windows/Linux 未验证状态、迁移/回滚、发布包不适用均明确记录。
