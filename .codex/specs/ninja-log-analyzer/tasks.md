# 实施计划：Ninja 构建日志分析器

> 阶段：tasks
>
> 状态：已执行完成
>
> 最近更新：2026-07-22

## 执行策略

- Required 任务构成最小完整交付；本轮无 Optional 任务。
- 按依赖波次顺序执行；虽然 TASK-002/TASK-003、TASK-006/TASK-007 可形成同一波次，但用户未要求并行代理，因此在同一任务中顺序推进。
- 每项实现同时添加直接测试或明确的 GUI 验证，不把测试全部推迟到最后。
- 规格冲突或代码事实变化时，重新打开对应上游阶段，再传播到设计、任务和验证证据。

## 执行波次

| 波次 | 任务 | 完成后可观察状态 |
|---|---|---|
| 1 | TASK-001 | CMake 可配置，核心/GUI/测试目标边界建立 |
| 2 | TASK-002、TASK-003 | 可定位并解析日志，可用 manifest/rule 或输出特征分类 |
| 3 | TASK-004 | 可生成批次、指标、排序和瓶颈提示 |
| 4 | TASK-005 | 桌面应用可选择路径并原子加载分析 |
| 5 | TASK-006、TASK-007 | 仪表盘、过滤、慢任务和交互时间线完整 |
| 6 | TASK-008 | 示例、文档、性能/兼容验证和规格审计完成 |

## 任务列表

- [x] TASK-001：建立跨 Qt 主版本的可测试工程骨架
  - 类型：required
  - 需求：REQ-002、REQ-004、REQ-006；NFR-003、NFR-004、NFR-006
  - 设计：DEC-001 / 建议目录结构 / 数据模型与状态
  - 依赖：无
  - 产出：根 `CMakeLists.txt`；`ninja_analyzer_core`、`ninja_log_analyzer`、`ninja_analyzer_tests` 目标；C++17/AUTOMOC/CTest 配置；核心公共值类型和最小应用入口；FACT-010 对应的窄范围 AGL compatibility guard。
  - 验证：分别以 Qt6.4.3 和 Qt5.15.2 的 prefix 配置独立 build 目录、完整链接应用并由 CTest 启动测试；确认 guard 只在 Qt6/AGL SDK 缺失组合生效。
  - 实施记录：已新增 `CMakeLists.txt`、`src/core/NinjaLogTypes.{h,cpp}`、最小 `src/main.cpp`、`src/gui/MainWindow.{h,cpp}`、`tests/tst_core.cpp`。Qt6 `build-qt6` 显示 guard 生效后应用链接成功，CTest 1/1 通过；Qt5 `build-qt5` 无 guard 消息且应用链接成功，CTest 1/1 通过。首次 Qt6 AGL 失败证据已传播到 FACT-010、DEC-001 和本任务契约。

- [x] TASK-002：实现路径发现与 v4/v5 日志容错解析
  - 类型：required
  - 需求：REQ-001、REQ-002；NFR-002、NFR-005
  - 设计：`LogLocator::resolve`、`NinjaLogParser::parse` / DEC-002 / PROP-001、PROP-002
  - 依赖：TASK-001
  - 产出：只读文件/目录解析、递归稳定候选列表、签名/字段/数值/哈希校验、行级警告和致命错误；QtTest 覆盖合法 v4/v5、CRLF、空格、坏行、尾行和错误路径。
  - 验证：`ctest --test-dir build -R ninja_analyzer_tests --output-on-failure`，其中 locator/parser 用例全部通过；测试确认输入内容未改变。
  - 实施记录：新增 `src/core/LogLocator.{h,cpp}`、`src/core/NinjaLogParser.{h,cpp}` 并更新核心 target；扩展 `tests/tst_core.cpp`。首次测试发现隐藏文件过滤缺失，加入 `QDir::Hidden` 后 Qt6/Qt5 均为 CTest 1/1 通过。覆盖 AC-001.1/2/4、AC-002.1—5、PROP-001/002；多候选 UI 选择留给 TASK-005。

- [x] TASK-003：实现 manifest 规则映射与可追溯步骤分类
  - 类型：required
  - 需求：REQ-003；NFR-002、NFR-004
  - 设计：DEC-004 / manifest 输出映射算法 / `NinjaManifestParser::loadNear` / PROP-004
  - 依赖：TASK-001
  - 产出：邻近 `build.ninja` 定位、有界 include/subninja、续行/输出转义解析、output→rule 映射、rule 优先/输出回退分类、匹配状态和警告；覆盖多输出、隐式输出、转义空格、缺失 manifest 的测试。
  - 验证：`ctest --test-dir build -R ninja_analyzer_tests --output-on-failure`；manifest/classification 用例全部通过。
  - 实施记录：新增 `src/core/NinjaManifestParser.{h,cpp}` 并更新核心 target/`tests/tst_core.cpp`。实现 4 级上行查找、32 文件递归上限、literal include/subninja、续行、显式/隐式多输出、`$ ` / `$:` / `$$` 转义、绝对/相对键匹配、十类 rule 分类和输出回退。Qt6/Qt5 CTest 均 1/1 通过，覆盖 AC-003.3—5、PROP-004。

- [x] TASK-004：实现批次、统计、排序和瓶颈洞察引擎
  - 类型：required
  - 需求：REQ-003、REQ-004、REQ-005；NFR-001、NFR-006
  - 设计：DEC-003、DEC-005 / 指标与并发算法 / 瓶颈提示 / PROP-003、PROP-005、PROP-006、PROP-007
  - 依赖：TASK-002、TASK-003
  - 产出：批次完整分区、当前视图分析、分类守恒、并发扫描、稳定慢任务排序、时长格式化和证据化洞察；单元测试覆盖回退、零耗时、重叠/相邻区间、守恒、排序和提示阈值。
  - 验证：`ctest --test-dir build -R ninja_analyzer_tests --output-on-failure`；小规模穷举并发对照通过；Release 10 万条核心计时小于 2 秒。
  - 实施记录：新增 `src/core/BuildAnalyzer.{h,cpp}` 并扩展核心 target/测试。实现时间回退批次、批次取数、摘要/分类统计、半开区间并发扫描、确定性慢任务排序、ms/s/min 格式和四类证据化洞察。Qt6 首次编译发现 `qsizetype→int` 窄化后改为显式安全转换；Qt6/Qt5 Debug CTest 均 1/1 通过。Qt6 Release 10 万条分析专项用例通过，测试进程总计 57 ms。覆盖 AC-003.1/2、AC-004.1—5、AC-005.3、PROP-003/005/006/007。

- [x] TASK-005：实现可恢复的 Qt 主窗口加载工作流
  - 类型：required
  - 需求：REQ-001、REQ-003、REQ-006；NFR-005
  - 设计：DEC-006 / MainWindow 状态机 / 关键流程 / PROP-008
  - 依赖：TASK-004
  - 产出：路径输入、选择日志、选择目录、分析按钮、多候选对话框、批次选择、加载状态/诊断、关于分析口径；流水线局部构造成功后才替换当前结果。
  - 验证：启动应用后用 demo 目录完成加载；随后输入坏路径，原结果保持；切换推断批次/全部记录可刷新；`QT_QPA_PLATFORM` 可用时执行 GUI 冒烟测试。
  - 实施记录：重写 `src/gui/MainWindow.{h,cpp}` 和启动参数处理，新增 `tests/tst_mainwindow.cpp`/`ninja_analyzer_gui_tests`。实现文件/目录选择、多候选交互、完整候选流水线后原子提交、默认最后推断批次/全部记录切换、格式/有效/忽略/manifest/批次诊断、分析口径说明和初始空态。Qt6/Qt5 均 core+offscreen GUI 2/2 通过；GUI 回归验证两批次加载、全量切换与失败保留成功状态，覆盖 AC-001.3、AC-003.2/5、AC-006.1/2/3/5、PROP-008。

- [x] TASK-006：实现总览、分类、慢任务和同步过滤
  - 类型：required
  - 需求：REQ-004、REQ-005、REQ-006
  - 设计：DEC-005 / `AnalysisResult`、`MainWindow` / AC-005.1、AC-005.3、AC-005.4
  - 依赖：TASK-005
  - 产出：任务数/观察窗口/累计时间/平均和最大并行度摘要卡；带占比的分类汇总；瓶颈提示；稳定慢任务表；输出搜索和类型过滤；空态和“汇总未过滤”说明。
  - 验证：demo 数据的卡片、分类总和、慢任务首项与核心测试预期一致；搜索/类型过滤只改变明细和时间线数据源；1280×760 及缩小窗口可访问核心区域。
  - 实施记录：新增 `src/gui/CategoryPalette.h`、`SlowTasksModel.{h,cpp}`，扩展 `MainWindow` 为总览/最慢步骤/时间线 tab。总览含五张摘要卡、六列分类表/彩色占比和解释性洞察；虚拟 model 保存全部慢任务，显示 output/type/rule/start/end/duration/source；类型与输出过滤只更新明细。Qt6/Qt5 均 2/2 通过，GUI 测试证明过滤后摘要保持完整口径、切换全量后刷新为 4 条，并在 820×600 显示 central widget。Qt6 弃用警告用 5/6 条件 API 消除。覆盖 AC-004.1—5、AC-005.1/3/4、AC-006.4。

- [x] TASK-007：实现限量、无重叠泳道的交互时间线
  - 类型：required
  - 需求：REQ-005、REQ-006；NFR-001
  - 设计：DEC-007 / `TimelineWidget` / PROP-009
  - 依赖：TASK-005
  - 产出：QPainter 时间轴、网格、类别颜色、泳道任务条、悬停 tooltip、空态、5000 条上限与截断提示；纯布局逻辑可测试。
  - 验证：泳道单元测试证明同 lane 的正时长区间不重叠且数量不超过 5000；demo GUI 中任务条位置、颜色、tooltip 和过滤刷新正确。
  - 实施记录：新增 `src/gui/TimelineWidget.{h,cpp}` 并接入 GUI targets/MainWindow。纯布局选择最慢 5000 条后按开始时间排序，复用最早可用 lane；QPainter 绘制相对时间网格、泳道背景、类别色任务条/标签、空态，鼠标悬停显示 output/type/rule/start/end/duration/source；状态区说明绘制/截断数。Qt6/Qt5 均 2/2 通过；布局测试证明同 lane 正时长区间不重叠、5002→5000 截断；GUI 测试证明过滤同步 2→1→4。覆盖 AC-005.2/4/5、PROP-009；demo 视觉检查归 TASK-008。

- [x] TASK-008：补齐示例、使用文档和端到端验收证据
  - 类型：required
  - 需求：REQ-001—REQ-006；NFR-001—NFR-006
  - 设计：测试策略 / 需求覆盖矩阵 / 非功能设计
  - 依赖：TASK-006、TASK-007
  - 产出：合成 demo `.ninja_log/build.ninja`、中文 README（构建/运行/统计口径/限制/FACT-010 guard）、Qt6/Qt5 完整构建测试、GUI 视觉检查、最终代码—规格覆盖证据。
  - 验证：Qt6 与 Qt5 独立 build 目录分别 `cmake --build` + `ctest --output-on-failure`；以 demo 路径启动应用并完成视觉检查；`validate_spec.py` 通过；所有 required 项有实施记录。
  - 实施记录：新增 `.gitignore`、中文 `README.md`、`examples/demo-build/.ninja_log` 与 `build.ninja`；新增 10 万条解析+分类+统计测试和按环境变量启用的 1280×760 总览/时间线快照测试。Qt6 Release 专项用例总计 198 ms（外部进程 real 0.67 s）；全新 `build-clean-qt6`/`build-clean-qt5` 均完整链接应用且 CTest 2/2 通过（1.19 s/1.23 s）。Qt6 `-Wall -Wextra -Wpedantic` 构建无项目警告且 CTest 2/2 通过。demo 视觉检查确认 v5、19 条、2 批次、manifest 19/19，最后批次 14 任务/6 s/18.1 s/平均 3.02/最大 7；总览和时间线在 1280×760 无重叠，溢出区域可滚动。截图位于会话 visualization 目录的 `ninja-log-analyzer-overview.png` 和 `ninja-log-analyzer-overview-timeline.png`。覆盖 REQ-001—006、NFR-001—006 的端到端与交付文档证据。

## 覆盖检查

| 行为 | 实现任务 | 验证任务/证据 | 状态 |
|---|---|---|---|
| REQ-001 | TASK-002、TASK-005 | locator 单元 + 多候选/原子 GUI 工作流 | 已验证 |
| REQ-002 | TASK-001、TASK-002 | v4/v5、坏行、CRLF、只读 parser 测试 | 已验证 |
| REQ-003 | TASK-003、TASK-004、TASK-005 | manifest/classifier/batch 单元 + GUI 状态 | 已验证 |
| REQ-004 | TASK-001、TASK-004、TASK-006 | 守恒/并发/排序单元 + demo 摘要/表格 | 已验证 |
| REQ-005 | TASK-004、TASK-006、TASK-007 | 洞察/泳道/截断/过滤测试 + 双快照 | 已验证 |
| REQ-006 | TASK-001、TASK-005、TASK-006、TASK-007 | offscreen GUI 恢复测试 + 1280×760/820×600 检查 | 已验证 |
| NFR-001 | TASK-004、TASK-007、TASK-008 | Release 10 万条 198 ms + 5000 绘制上限 | 已验证 |
| NFR-002 | TASK-002、TASK-003、TASK-008 | 错误/坏行/零值/缺 manifest 测试 | 已验证 |
| NFR-003 | TASK-001、TASK-008 | Qt6/Qt5 全新 Release 构建与 CTest | 已验证 |
| NFR-004 | TASK-001、TASK-003、TASK-008 | 共同 API 审查 + 双 Qt + 严格警告构建 | 已验证（Windows/Linux 源码兼容未实机） |
| NFR-005 | TASK-002、TASK-005、TASK-008 | 输入字节不变测试 + 无写入业务接口审查 | 已验证 |
| NFR-006 | TASK-001、TASK-004、TASK-008 | Core/Widgets target 边界 + 核心自动化测试 | 已验证 |

## 完成门槛

- [x] 所有 required 任务完成。
- [x] REQ-001—REQ-006 和 PROP-001—PROP-009 均有自动化或明确人工验证证据。
- [x] Qt6 本机构建、全部 CTest、demo GUI 冒烟和视觉检查通过。
- [x] Qt6（含有边界 AGL guard）和 Qt5（无该 guard）均完成配置、构建和测试。
- [x] 性能、兼容性、只读、错误恢复和时间线截断均已验证或明确接受风险。
- [x] `validate_spec.py` 和代码—规格一致性审计通过。
