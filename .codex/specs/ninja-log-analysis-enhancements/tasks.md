# 实施计划：ninja-log-analysis-enhancements

> 阶段：tasks
>
> 状态：草案
>
> 最近更新：2026-07-24

## 执行策略

- Required 任务构成最小完整交付，按依赖顺序执行，每项先测试再回写证据。
- 保持 `app → gui → application → core`；新增源文件只在所属模块 CMake 中声明。
- 每次业务改动前说明范围；实现与 Spec 冲突时先更新上游文档。
- 最终以 Qt 5.15.2、Qt 6.4.3、CTest、macOS bundle/启动检查为完成门槛。

## 任务列表

- [x] TASK-001：支持 v7 并按内容定位任意名称日志
  - 类型：required
  - 需求：REQ-001
  - 设计：DEC-001 / PROP-001 / PROP-002
  - 单一变更原因：解除版本和文件名限制，同时保持严格格式校验。
  - 模块/构建单元：core / `ninja_analyzer_core`
  - 架构约束：ARCH-001、ARCH-002 / BUILD-001
  - 依赖变化：无新增依赖；继续只依赖 Qt Core。
  - 平台/交付物：平台无关源码行为；不单独产生交付物。
  - 依赖：无
  - 修改范围：`src/core/NinjaLogTypes.h`、`NinjaLogParser.cpp`、`LogLocator.cpp`、`tests/tst_core.cpp`，必要的引用点。
  - 产出：v4/v5/v7 解析、`hasCommandHash`、128 字节内容探测、稳定目录候选。
  - 验证：core 测试覆盖 v4/v5/v7/v6/坏字段/任意名/目录/有界探测。
  - 实施记录：2026-07-24 完成。`ninja_analyzer_tests`（Qt 6）通过；新增 v7、v6 拒绝、任意名称文件、混合目录测试，v4/v5 回归通过。

- [x] TASK-002：采集并传递分析时机器负载快照
  - 类型：required
  - 需求：REQ-004
  - 设计：DEC-004 / PROP-005
  - 单一变更原因：为一次成功加载建立可复用、诚实降级的机器上下文。
  - 模块/构建单元：application / `ninja_analyzer_application`
  - 架构约束：ARCH-001、ARCH-003 / BUILD-001
  - 依赖变化：application 内部新增 OS API 条件编译；无 Qt Widgets 依赖。
  - 平台/交付物：macOS/Linux/FreeBSD 尝试 load average；其他平台显式不可用；不单独产生交付物。
  - 依赖：TASK-001
  - 修改范围：新增 `MachineLoadSnapshot.*`/`MachineLoadProbe.*`，扩展 `LoadedAnalysis`、`AnalysisService`、application CMake 与测试。
  - 产出：采集时间、逻辑处理器、系统/架构、1/5/15 分钟 load average 或不可用状态。
  - 验证：application 单元测试快照必需字段、可用/降级格式和成功加载持有快照。
  - 实施记录：2026-07-24 完成。新增稳定快照/探针，成功加载后保存采集时间、逻辑处理器、平台和可用时的 load average；application 测试通过。

- [x] TASK-003：实现应用层完整 HTML 报告导出
  - 类型：required
  - 需求：REQ-003、REQ-004
  - 设计：DEC-003、DEC-004 / PROP-004、PROP-005
  - 单一变更原因：提供独立于 GUI 和筛选状态的可测试完整报告。
  - 模块/构建单元：application / `ninja_analyzer_application`
  - 架构约束：ARCH-001、ARCH-003 / BUILD-001
  - 依赖变化：无新增外部依赖；使用 Qt Core `QSaveFile`/`QTextStream`。
  - 平台/交付物：平台无关；运行时由用户选择生成单文件 `.html`。
  - 依赖：TASK-002
  - 修改范围：新增 `AnalysisReportExporter.*`、application CMake、`tests/tst_application.cpp`。
  - 产出：原子 UTF-8 HTML，包含元信息、五项摘要、分类、洞察、泳道/负载说明与全部任务。
  - 验证：完整字段、记录数、排序、HTML 转义、空/不可写路径失败测试。
  - 实施记录：2026-07-24 完成。`QSaveFile` 原子 UTF-8 HTML 覆盖全部规定章节；完整性、HTML 转义和失败请求测试通过。

- [x] TASK-004：让时间线完整滚动并解释泳道
  - 类型：required
  - 需求：REQ-002
  - 设计：DEC-002 / PROP-003
  - 单一变更原因：修复下方泳道不可达并消除泳道等于核心数的误解。
  - 模块/构建单元：gui / `ninja_analyzer_gui`
  - 架构约束：ARCH-001、ARCH-004 / BUILD-001
  - 依赖变化：无；沿用 Qt Widgets。
  - 平台/交付物：平台无关 UI 行为；最终进入各平台应用。
  - 依赖：TASK-001
  - 修改范围：`TimelineWidget.*`、`TimelinePage.*`、必要的 AppStyle、GUI 测试。
  - 产出：动态内容高度、垂直滚动范围、泳道定义/数量/限制说明。
  - 验证：24 个重叠区间出现滚动范围、收缩后范围更新、说明文案断言。
  - 实施记录：2026-07-24 完成。时间线最小高度随泳道数增长/收缩；24 个完全重叠区间的滚动最大值、滚到底和收缩归零测试通过。

- [x] TASK-005：集成可见报告入口、任意文件选择和机器上下文
  - 类型：required
  - 需求：REQ-001、REQ-002、REQ-003、REQ-004
  - 设计：DEC-001–DEC-004 / PROP-004、PROP-005
  - 单一变更原因：把已验证的核心/应用能力连接为完整用户旅程。
  - 模块/构建单元：gui / `ninja_analyzer_gui`
  - 架构约束：ARCH-001、ARCH-004 / BUILD-001
  - 依赖变化：gui 使用 application 新公开的 snapshot/report 契约；不新增反向依赖。
  - 平台/交付物：进入 macOS `<build>/bin/Ninja Log Analyzer.app` 及 Windows/Linux 源码构建物。
  - 依赖：TASK-002、TASK-003、TASK-004
  - 修改范围：`MainWindow.*`、`AppStyle.cpp`、GUI CMake/测试、示例日志（如需）。
  - 产出：最小窗口可见且状态正确的导出按钮、任意文件选择文案、机器负载卡片/说明、保存反馈、筛选无关完整导出。
  - 验证：820px 宽窗口按钮初始禁用/加载后启用；筛选 1/N 后报告仍 N；UI 快照与报告一致。
  - 实施记录：2026-07-24 完成。顶部导出按钮、任意文件选择、v4/v5/v7 文案、负载卡片和保存反馈已接入；820×600 可见性及筛选 1/2 仍导出 2 条任务通过。

- [x] TASK-006：更新文档并完成双 Qt 与 macOS 交付验证
  - 类型：required
  - 需求：REQ-001、REQ-002、REQ-003、REQ-004；NFR-001–NFR-006
  - 设计：BUILD-001、BUILD-002 / 全部正确性属性
  - 单一变更原因：形成可复现证据并保证增强未破坏交付契约。
  - 模块/构建单元：文档、测试、app 交付
  - 架构约束：ARCH-001 / BUILD-001、BUILD-002
  - 依赖变化：无。
  - 平台/交付物：macOS `<build>/bin/Ninja Log Analyzer.app`；Windows/Linux 仅记录源码兼容限制。
  - 依赖：TASK-005
  - 修改范围：`README.md`、必要的测试/CMake/示例、三份 Spec 实施记录。
  - 产出：六项功能使用说明、泳道/负载限制、报告位置说明和完整验证证据。
  - 验证：Qt5/Qt6 configure/build/CTest、严格警告、bundle runtime 检查和非交互启动。
  - 实施记录：2026-07-24 完成。README/示例更新；Qt 5.15.2 与 Qt 6.4.3 各 4/4 CTest 通过；两套 bundle 验证通过；Qt6 `.app` 使用打包的 cocoa plugin 原生启动成功。Windows/Linux 未在原生 runner 验证。

## 覆盖检查

| 行为 | 实现任务 | 验证任务/证据 | 状态 |
|---|---|---|---|
| REQ-001 | TASK-001、TASK-005 | core/GUI 测试 | 已通过 |
| REQ-002 | TASK-004、TASK-005 | GUI 滚动/说明测试 | 已通过 |
| REQ-003 | TASK-003、TASK-005 | application/GUI 导出测试 | 已通过 |
| REQ-004 | TASK-002、TASK-003、TASK-005 | application/GUI 快照一致性测试 | 已通过 |
| NFR-001–006 | TASK-006 | 双 Qt、CTest、bundle/启动 | 已通过（Windows/Linux 原生验证不适用/未宣称） |

## 完成门槛

- [x] 所有 required 任务完成
- [x] 所有行为和正确性属性均有验证证据
- [x] 相关回归与项目标准检查通过
- [x] macOS `.app` 的精确产物、运行时依赖和启动方式已在本机验证
- [x] Windows/Linux 未验证项被明确记录，不虚构证据
- [x] README 与 requirements/design/tasks 和实际 UI 一致
