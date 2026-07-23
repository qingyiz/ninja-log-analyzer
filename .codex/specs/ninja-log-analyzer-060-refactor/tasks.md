# 实施计划：Ninja 日志分析器 0.6.0 架构重构

> 阶段：tasks
>
> 状态：已执行完成
>
> 最近更新：2026-07-24

## 执行策略

- 所有 9 项均为 required；按依赖顺序执行，每项实现与直接测试同批完成。
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
| 6 | TASK-006 | macOS bundle 改为 build 根级唯一产物并重新闭环 |
| 7 | TASK-007 | `build/bin` 直接生成自包含 macOS bundle 并重新闭环 |
| 8 | TASK-008 | macOS build/install bundle 携带并声明项目自有图标 |
| 9 | TASK-009 | 筛选组合框与结果标签栏使用统一的项目视觉样式 |

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
  - 产出：顶层 <=45 行；core/application/gui/app/tests 就近声明；AGL guard 在单责 module；输出根为 `<build>/bin`，macOS 的最终自包含语义由 TASK-007 修正规则负责。
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
  - 平台/交付物：历史实施的 macOS arm64 开发路径为 `<build>/bin/Ninja Log Analyzer.app`，该路径经用户反馈被 TASK-006 废弃；部署 `<stage>/Ninja Log Analyzer.app` 仍有效；Windows/Linux 仅通用 runtime install 契约且未验证。
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

- [x] TASK-006：修正 macOS 开发 bundle 的 build 根级路径
  - 类型：required
  - 需求：REQ-004；NFR-002、NFR-004
  - 设计：DEC-003 / BUILD-003 / macOS 应用束约束 / PROP-005、PROP-006
  - 单一变更原因：消除 `build/bin` 与用户要求的 build 根目录之间的精确路径偏差，并防止旧路径再次出现。
  - 模块/构建单元：`ninja_log_analyzer` + `NinjaAnalyzerDelivery`。
  - 架构约束：遵守 BUILD-003；只改 app/delivery/test 契约，不向顶层 CMake 或业务模块加入平台细节。
  - 依赖变化：无生产 include/link 变化；新增 build-tree bundle 路径验证脚本和 CTest。
  - 平台/交付物：macOS arm64 开发 `<build>/Ninja Log Analyzer.app`；部署 `<stage>/Ninja Log Analyzer.app`；Windows/Linux 继续 `<build>/bin` 且本机未验证。
  - 依赖：TASK-005
  - 修改范围：`src/app/CMakeLists.txt`、`cmake/NinjaAnalyzerDelivery.cmake`、新增精确路径检查脚本、`tests/CMakeLists.txt`、`README.md` 和本 Spec；不改业务源码、GUI、core/application 依赖。
  - 产出：macOS 单/多配置输出属性指向 build 根；构建后自动验证 bundle 结构和路径唯一性；README 命令与实际一致。
  - 验证：先清理旧 build target 产物，再在 `build` 目录重新配置/构建；断言根级 `.app` 存在且 `build/bin` 无同名 bundle；CTest；`verify_delivery.py`；install 自包含检查；cocoa 启动；Qt5/Qt6 回归；Spec validate/complete。
  - 实施记录：确认原实现实际生成 `build/bin/Ninja Log Analyzer.app` 后，按用户明确要求把 macOS 单/多配置 `RUNTIME_OUTPUT_DIRECTORY` 改为 build 根，Windows/Linux 继续使用 `bin`。新增 post-build 与 CTest 共用的 `NinjaAnalyzerVerifyMacBundle.cmake`，同时断言根级 bundle 的 Info.plist/主程序和旧 `bin` bundle 不存在；守卫首次运行成功发现旧残留并阻止假通过，随后只删除该可重建旧 bundle。实际 `build` 目录现仅有 `build/Ninja Log Analyzer.app`，Qt5 Debug CTest 4/4（1.65s）。全新 Qt6 Release CTest 4/4（1.86s）、Qt5 Release CTest 4/4（1.16s）；两套 build bundle 通过 `verify_delivery.py`，两套 install bundle 通过 `--require-self-contained`，包含 cocoa plugin 与正确 LC_RPATH；Qt5/Qt6 部署应用均通过加载 demo 的 cocoa 启动 smoke。覆盖 AC-004.1、PROP-005/006。

- [x] TASK-007：让 `build/bin` 直接产生完整 macOS bundle
  - 类型：required
  - 需求：REQ-004；NFR-002、NFR-004
  - 设计：DEC-003 / BUILD-003 / macOS 应用束约束 / PROP-005、PROP-006
  - 单一变更原因：落实用户最新明确的 `build/bin` 路径，并消除 build bundle 缺 Qt runtime、只有 stage bundle 完整的偏差。
  - 模块/构建单元：`ninja_log_analyzer` + `NinjaAnalyzerDelivery`。
  - 架构约束：遵守 BUILD-003；部署/验证细节留在 `cmake` module，不进入业务源码或顶层编排。
  - 依赖变化：无生产 include/link 变化；app target 的 POST_BUILD 增加 active Kit `macdeployqt` 和自包含结构检查。
  - 平台/交付物：macOS arm64 `<build>/bin/Ninja Log Analyzer.app` 为主自包含交付物；`<stage>/Ninja Log Analyzer.app` 为安装副本。
  - 依赖：TASK-006
  - 修改范围：`src/app/CMakeLists.txt`、delivery/部署/验证 CMake 脚本、`tests/CMakeLists.txt`、`README.md` 与本 Spec；不改业务代码、GUI、core/application 依赖。
  - 产出：默认 build 后 `bin` 中含 Frameworks、cocoa plugin、正确 RPATH 的唯一 `.app`；部署失败使 build 失败。
  - 验证：Qt6/Qt5 全新 build+CTest；两套 build bundle 执行 `verify_delivery.py --require-self-contained`；build bundle 加载 demo 启动 smoke；install 副本复验；inspect/validate/git diff 审计。
  - 实施记录：将 macOS 单/多配置 app 输出统一为 `<build>/bin/Ninja Log Analyzer.app`，并在 app 的 POST_BUILD 中使用当前 Qt Kit 的 `macdeployqt` 收集 Frameworks/plugins；部署失败或缺 Frameworks、cocoa plugin、bundle RPATH、精确路径不符时直接使 build 失败。app target 显式统一 build/install RPATH 为 `@executable_path/../Frameworks`，install 只复制并复验完整 bundle，消除 CMake 对已改写旧 RPATH 的二次删除错误。全新 Qt5 Release 和 Qt6 Release 均在 `bin` 生成唯一 bundle，CTest 各 4/4 通过（Qt5 1.41s；Qt6 最新复验 0.65s）；两套 build bundle 和两套最新 install 副本均通过 0.6.0 `verify_delivery.py --require-self-contained`，包含 Qt Frameworks、`libqcocoa.dylib`、正确 LC_RPATH、arm64 主程序和 macOS 11.0 元数据。Qt5/Qt6 build bundle 加载 demo 后均持续运行 3 秒并受控退出，cocoa 启动 smoke 通过。`inspect_structure.py` 确认顶层仍为 38 行纯编排，生产文件无新复杂度触发；`git diff --check` 通过。覆盖 AC-004.1—4、PROP-005/006。

- [x] TASK-008：为 macOS bundle 增加项目自有图标
  - 类型：required
  - 需求：REQ-004；NFR-002、NFR-004
  - 设计：DEC-003 / BUILD-003 / macOS 应用束约束 / PROP-007
  - 单一变更原因：修复 build/install 应用显示系统默认图标的问题。
  - 模块/构建单元：`ninja_log_analyzer` app target。
  - 架构约束：遵守 BUILD-003；图标资源就近归 app target 所有，delivery module 只验证最终 bundle，不向业务或顶层构建入口加入资源细节。
  - 依赖变化：无 include/link 变化；app target 新增 `.icns` bundle resource。
  - 平台/交付物：macOS build `<build>/bin/Ninja Log Analyzer.app/Contents/Resources/NinjaLogAnalyzer.icns` 与 install 副本；Windows/Linux 无变化。
  - 依赖：TASK-007
  - 修改范围：新增 `resources/icons` 图标母版/`.icns`，更新 app CMake、Info.plist、bundle 验证脚本、README 与本 Spec；不改业务代码和 UI。
  - 产出：Finder/Dock 可识别的项目自有图标；标准 16—1024 像素层级；plist/资源一致。
  - 验证：`iconutil` 展开层级；Qt5/Qt6 build+CTest；build/install bundle 检查 `CFBundleIconFile` 与资源；`verify_delivery.py --require-self-contained`；启动 smoke；图标预览。
  - 实施记录：使用内置图像生成工具制作 1254px 母版，在纯色背景上生成深蓝圆角方形与三条并行构建线/速度切线标记；经官方 imagegen helper 去除背景并缩放为带 alpha 的 1024px PNG。使用 `sips` 生成 16、32、64、128、256、512、1024 像素表示并由 `iconutil` 打包为 `NinjaLogAnalyzer.icns`，反向展开确认 10 个标准 iconset 文件齐全，64px 与 bundle 内实际 icns 渲染目视清晰。app target 通过 `MACOSX_PACKAGE_LOCATION=Resources` 携带图标，Info.plist 声明 `CFBundleIconFile=NinjaLogAnalyzer.icns`；bundle 验证脚本检查 plist、资源，并用 `iconutil` 自动展开全部标准层级。Qt5/Qt6 全套 CTest 各 4/4 通过（2.26s/2.44s），新增图标交付测试复验分别 0.18s/0.17s；两套 build 与 install bundle 均存在图标并通过 `verify_delivery.py --require-self-contained`。Qt5/Qt6 应用加载 demo 后持续运行 3 秒，cocoa smoke 通过。Quick Look 服务在当前桌面会话阻塞，已中止；以 bundle 内 icns 的 `sips` 渲染和 plist/iconutil 自动证据替代，不影响 required 验收。`inspect_structure.py`、`git diff --check` 通过。覆盖 AC-004.6、PROP-007。

- [x] TASK-009：统一筛选组合框与结果标签栏视觉
  - 类型：required
  - 需求：REQ-002 / AC-002.5；NFR-002、NFR-003、NFR-004
  - 设计：DEC-004 / AppStyle、AnalysisResultsWidget / ARCH-002、BUILD-002 / PROP-008
  - 单一变更原因：消除 macOS 平台原生组合框和标签子控件与项目视觉体系混用的问题。
  - 模块/构建单元：`ninja_analyzer_gui`。
  - 架构约束：遵守 ARCH-002、BUILD-002；视觉资源和规则归 presentation target，就近编译，不修改 application/core 或顶层构建入口。
  - 依赖变化：无生产层级依赖变化；GUI target 新增 Qt resource 输入。
  - 平台/交付物：平台无关 Qt Widgets UI；在 macOS arm64 Qt5/Qt6 原生渲染验证；最终仍由现有 `.app` 携带。
  - 依赖：TASK-008
  - 修改范围：`src/gui/AppStyle.cpp`、`src/gui/AnalysisResultsWidget.cpp`、`src/gui/CMakeLists.txt`、新增 `resources/ui` SVG/qrc、`tests/tst_mainwindow.cpp` 与本 Spec；不改数据、筛选、页面内容和交付路径。
  - 产出：统一圆角组合框、项目 chevron、弹出列表状态和圆角分段标签栏；原有交互及计数保持。
  - 验证：Qt5/Qt6 GUI CTest；全套 CTest；demo 视觉快照；结构审计、Spec validate、`git diff --check`。
  - 实施记录：`AppStyle` 补齐 `QComboBox` 的独立内边距、hover/focus、drop-down、down-arrow 和 popup item 规则，新增 12×8 SVG chevron；GUI target 就近编译 qrc，并由 `AppStyle` 显式初始化，避免静态库资源被链接器裁剪。`AnalysisResultsWidget` 为内部 tab bar 设置 `resultTabBar` 对象名并关闭原生 base，只在该标签栏上应用轻灰圆角容器、白色选中片和紫色选中态，消除用户截图中的右侧黑线与灰色直角块。GUI 回归新增资源存在、对象名、关键规则和 tab 切换检查。Qt5/Qt6 GUI CTest 分别通过（0.67s/1.52s），完整 CTest 均 4/4 通过（0.85s/0.75s）；两套 Cocoa demo 快照目视确认组合框和标签栏一致，实际 Qt5/Qt6 `.app` 加载 demo 后均持续运行 3 秒。两套 build bundle 再次通过 0.6.0 `verify_delivery.py --require-self-contained`。覆盖 AC-002.5、PROP-008。

## 覆盖检查

| 行为 | 实现任务 | 验证任务/证据 | 状态 |
|---|---|---|---|
| REQ-001 | TASK-001 | application/core tests、demo 19/2/14 基线 | 已验证 |
| REQ-002 | TASK-002、TASK-009 | Qt5/Qt6 offscreen GUI tests、双 Kit 启动和视觉快照 | 已验证 |
| REQ-003 | TASK-003 | target graph、独立 targets、inspect、严格警告 build | 已验证 |
| REQ-004 | TASK-004、TASK-006、TASK-007、TASK-008 | `build/bin` 自包含 bundle、图标、双 Qt build/install、verify_delivery 和 cocoa smoke | 已验证（macOS arm64） |

## 完成门槛

- [x] 所有 required 任务完成。
- [x] REQ-001—004 与 PROP-001—008 均有最新验证证据。
- [x] Qt6/Qt5 构建与全部 CTest 通过，性能门槛保持。
- [x] macOS `build/bin` bundle 的路径、图标、结构、依赖、架构和启动均有原生证据。
- [x] 0.6.0 inspect/validate、架构依赖与代码—规格一致性审计通过。
- [x] Windows/Linux 未验证状态、迁移/回滚、发布包不适用均明确记录。
