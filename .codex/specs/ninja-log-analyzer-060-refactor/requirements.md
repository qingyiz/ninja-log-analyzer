# 需求文档：Ninja 日志分析器 0.6.0 架构重构

> 阶段：requirements
>
> 工作流：design-first
>
> 状态：已完成
>
> 最近更新：2026-07-24

## 事实与环境基线

| ID | 事实或未知项 | 状态 | 证据/来源 | 对需求的影响 |
|---|---|---|---|---|
| FACT-001 | 按 spec-driven-development 0.6.0 重构，功能差不多即可，验证后提交 GitHub | 用户明确 | 2026-07-23 用户请求 | 保持行为，优先架构/构建/交付 |
| FACT-002 | 旧功能 Spec 与全部 required 任务已完成 | 已验证 | `.codex/specs/ninja-log-analyzer/*` | 旧 REQ-001—006 是行为回归基线 |
| FACT-003 | 旧 Spec 不通过 0.6.0 新校验 | 已验证 | 0.6.0 `validate_spec.py` 输出 | 必须补齐 ARCH/BUILD/复杂度/任务契约 |
| FACT-004 | `MainWindow.cpp` 903 行，顶层 CMake 136 行且多职责 | 已验证 | 0.6.0 `inspect_structure.py .` | 触发职责和构建拆分 |
| FACT-005 | 本机为 macOS 15.7.5 arm64，Qt 6.4.3/5.15.2、CMake 3.27.1、Ninja 1.11.1 | 已验证 | 只读版本探测、build caches | 本机原生验证两套 Qt 的 macOS app |
| FACT-006 | 现有 build `.app` 未收集 Qt runtime，旧 CMake 只有 `install(TARGETS)` | 已验证 | `otool -L`、bundle 结构、CMakeLists | 新增部署产物契约与自包含验证 |
| FACT-007 | Windows/Linux 当前没有原生 runner 或产物证据 | 未知 | 仓库无 CI，当前 host 为 macOS | 仅保持源码兼容，不宣称验证 |
| FACT-008 | 先前根据“build 目录”反馈把 macOS bundle 从 `build/bin` 改到了 build 根目录 | 已验证 | commit `9915cb3`、原 TASK-006 | 该解释与用户最新明确路径不一致 |
| FACT-009 | 用户最新明确要求 `build/bin` 下是完整 macOS 包 | 用户明确 | 2026-07-23 用户反馈 | build-tree 主交付物必须固定为 `<build>/bin/Ninja Log Analyzer.app` 且自包含 |
| FACT-010 | 当前 build-tree bundle 只有 Info.plist/主程序，缺 Qt Frameworks、cocoa plugin，且 LC_RPATH 指向开发机 Qt5 | 已验证 | `du`、`find`、`otool -L/-l`、0.6.0 `verify_delivery.py --require-self-contained` | 只改输出路径不够，默认 build 必须执行 Qt runtime 部署 |
| FACT-011 | 当前仓库没有图标资产，Info.plist 未声明 `CFBundleIconFile`，app target 也没有 bundle 资源 | 用户明确/已验证 | 2026-07-23 用户反馈；资源搜索；`src/app/CMakeLists.txt`、`cmake/NinjaAnalyzerInfo.plist.in` | macOS bundle 必须新增原生 `.icns` 并建立资源交付验证 |
| FACT-012 | 筛选栏组合框仍显示原生下拉子控件，结果标签栏使用灰色直角块，与现有白色圆角卡片和紫色主色体系不一致 | 用户明确/已验证 | 2026-07-24 用户截图；`src/gui/AppStyle.cpp`、`src/gui/AnalysisResultsWidget.cpp` | 补齐组合框子控件、弹出列表和结果标签栏的统一视觉契约 |

### 技术与运行环境调查

| 对象 | 探测方法 | 结果 | 结论 |
|---|---|---|---|
| CMake/Qt/C++ | 配置文件、版本命令 | C++17、CMake 3.20+、Qt5/6 Widgets | 已确认 |
| 目标平台/CPU/最低版本 | `sw_vers`、`uname`、framework `otool -l` | required macOS arm64；共同 Kit 下界 11.0 | 已确认本轮契约 |
| 开发/部署/发布 | 旧 build、CMake install、用户请求 | build `.app`；部署需自包含；不要求 DMG/签名 | 已确认/发布包不适用 |

## 问题与目标

### 问题陈述

现有软件功能完整，但窗口壳同时承担文件用例、三类页面渲染、筛选和样式，构建入口也集中声明全部模块、平台兼容、安装和测试。旧 Spec 的完成状态无法证明 0.6.0 新增的模块依赖、构建单元、复杂度预算和平台最终产物契约。

### 目标与成功指标

- 所有旧 core 与 GUI 自动化测试继续通过，demo 分析指标和交互保持。
- `MainWindow` 不再直接 include/call parser、manifest、analyzer；每个结果页独立拥有视图。
- 顶层 CMake 只负责编排，GUI 源码不在 app/test 中重复列出。
- Qt6 与 Qt5 分别完成干净配置、build、CTest；macOS build bundle 必须位于 `<build>/bin/Ninja Log Analyzer.app`，且默认 build 后即通过自包含检查。
- macOS build/install bundle 必须显示项目自有图标，不能回退为系统默认应用图标。
- 筛选栏与结果标签栏应使用一致的圆角、边框、留白和紫色交互状态，不混入原生黑色分隔线或大块灰色标签背景。
- 0.6.0 `inspect_structure.py`、`validate_spec.py` 和 Spec complete 全部通过。

### 非目标

- 不新增日志格式、指标、页面、后台线程或网络能力。
- 不重写稳定 core 算法，不改变统计口径。
- 不制作 DMG/PKG，不签名或公证。
- 不把 Windows/Linux 源码兼容写成原生交付已验证。

## 角色、术语与范围

### 角色

| 角色 | 目标 | 权限或限制 |
|---|---|---|
| Ninja/Qt 开发者 | 继续用现有分析功能定位构建瓶颈 | 本地只读日志 |
| 项目维护者 | 能按模块修改页面/用例/构建而不触发连锁修改 | CMake target 依赖必须显式 |
| macOS 使用者 | 获得结构正确且包含 Qt runtime 的 `.app` | 本轮不含发布签名 |

### 术语

| 术语 | 精确定义 |
|---|---|
| 保持行为 | 旧 Spec REQ-001—006 的用户可观察输入、输出、错误恢复和统计口径 |
| 开发/部署 bundle | CMake build tree 的 `bin` 中已收集 Qt frameworks/plugins、可直接启动并通过自包含检查的 `.app` |
| 安装副本 | install tree 中从完整 build bundle 安装并再次核验部署依赖的 `.app` |
| 发布包 | DMG/PKG/签名公证产物；本轮不适用 |

### 系统边界与依赖

- 范围内：application 用例层、presentation 页面拆分、target/CMake 模块化、macOS bundle/install/deploy、测试和文档。
- 范围外：core 算法功能扩展、发布渠道、远程服务。
- 外部依赖：C++17、CMake、Qt Core/Widgets/Test、macOS 部署时 active Kit 的 `macdeployqt`。

## 用户旅程

1. 用户仍从 `.ninja_log` 或目录开始，必要时选择一个候选。
2. 成功后仍查看默认最后批次的概览、慢任务和时间线，并可切换批次/过滤。
3. 失败或取消仍保留最近成功结果。
4. 维护者可以分别构建 core/application/gui/app/test targets。
5. macOS 维护者完成默认 build 后，直接在 `build/bin` 获得已收集 Qt runtime 的 `.app`；需要独立前缀时再 install 到 stage。
6. 用户在 Finder、Dock 或应用切换器中看到 Ninja Log Analyzer 自有图标，而不是默认应用图标。

## 功能需求

### REQ-001：保持分析加载结果

**用户故事：** 作为现有用户，我希望重构后相同日志得到相同分析，从而无需重新学习或怀疑数据口径。

- 优先级：Must
- 前置条件：输入符合旧 Spec 支持范围。
- 结果/副作用：只读产生等价分析，不修改输入。

#### 验收标准

- AC-001.1：当 service 加载合法单日志时，系统应当返回与现有 pipeline 相同的版本、有效/忽略数、manifest 匹配、records 和 batches。
- AC-001.2：当加载 demo 时，系统应当仍显示 19 条有效记录、2 个推断批次，并默认分析最后 14 个任务。
- AC-001.3：如果定位或解析失败，系统应当返回可操作错误且不产生可提交的半成品结果。

### REQ-002：保持可恢复的桌面交互

**用户故事：** 作为日常用户，我希望页面拆分后原有交互仍一致，从而重构不影响使用。

- 优先级：Must
- 前置条件：应用启动。
- 结果/副作用：单窗口显示和筛选状态更新。

#### 验收标准

- AC-002.1：成功加载后，系统应当同步更新诊断、批次、概览、慢任务和时间线。
- AC-002.2：当类型或路径过滤变化时，慢任务行数、时间线输入和两个 tab 计数应当一致，完整摘要保持当前批次口径。
- AC-002.3：在已有成功结果下，如果新加载失败或候选选择取消，系统应当保持当前日志、批次和页面结果。
- AC-002.4：当概览图/表发出类型或任务下钻时，系统应当更新过滤并导航到慢任务页。
- AC-002.5：在 macOS Qt5/Qt6 界面中，批次与类型组合框应使用项目自有下拉箭头、统一圆角边框和悬停/聚焦状态；结果标签栏应呈现圆角分段样式，选中项使用紫色强调，且不得出现原生黑色分隔线或不一致的灰色直角块。

### REQ-003：建立可执行的模块与构建边界

**用户故事：** 作为维护者，我希望职责和 target 依赖一致，从而可以局部修改和验证。

- 优先级：Must
- 前置条件：CMake 配置可找到 Qt5 或 Qt6。
- 结果/副作用：模块可独立构建，顶层只编排。

#### 验收标准

- AC-003.1：系统应当提供 core、application、gui、app 独立 target，依赖方向为 app→gui→application→core，禁止反向/循环依赖。
- AC-003.2：GUI 测试应当链接生产 GUI target，不得重复列举其 `.cpp`。
- AC-003.3：顶层 CMake 应当只保留工程、全局语言/Qt/CTest 设置和子目录/规则编排；模块、测试、平台部署细节就近声明。
- AC-003.4：结构审计不应再报告 MainWindow 500+ 行职责触发或顶层构建多职责触发。

### REQ-004：交付可验证的 macOS 应用 bundle

**用户故事：** 作为 macOS 使用者，我希望拿到结构正确且包含 Qt 依赖的应用，从而不依赖开发目录启动。

- 优先级：Must
- 前置条件：macOS arm64、active Qt Kit 提供 `macdeployqt`。
- 结果/副作用：build tree 直接产生完整 bundle，install tree 可产生副本；不签名公证。

#### 验收标准

- AC-004.1：在 macOS 上执行默认构建后，系统应当在 `<build>/bin/Ninja Log Analyzer.app` 生成 arm64 bundle；`<build>` 根目录不得存在第二份同名 `.app`。
- AC-004.2：bundle Info.plist 应当包含标识、显示名、0.2.0 版本、可执行名和最低 macOS 11.0。
- AC-004.3：`<build>/bin` bundle 应当包含 Qt frameworks 与 cocoa platform plugin；install 后 `<stage>/Ninja Log Analyzer.app` 应保持同一完整性。
- AC-004.4：`<build>/bin` bundle 的非系统依赖不得解析到开发机 Qt 绝对路径，并应当能直接启动加载 demo。
- AC-004.5：如果是 Windows/Linux 配置，系统应当保持可执行 target 和通用 install 规则，但只有原生 runner 验证后才能标记该平台已交付。
- AC-004.6：macOS build/install bundle 应当包含 `Contents/Resources/NinjaLogAnalyzer.icns`；Info.plist 的 `CFBundleIconFile` 应当引用该文件，且 `.icns` 应包含 16、32、128、256、512、1024 像素的标准图标表示。

## 非功能需求

| ID | 类别 | 可测约束 | 测量方式 |
|---|---|---|---|
| NFR-001 | 性能 | 10 万记录核心解析+统计仍小于 2 秒 | 现有 Release QtTest |
| NFR-002 | 可靠性 | Qt6/Qt5 全部现有 CTest 通过，失败状态不覆盖 | 双 Kit 干净构建/CTest |
| NFR-003 | 可维护性 | 生产 `.cpp` <=500 行或有单职责说明；顶层 CMake <=45 行且无 target 源码清单 | inspect + 人工职责审计 |
| NFR-004 | 兼容性 | 仅用 Qt5.15/Qt6.4 共同 API；Windows/Linux 无平台专有业务代码 | 双 Kit编译 + 静态审查 |
| NFR-005 | 隐私 | 不新增网络、日志写回或构建目录写入 | dependency/代码审查 |

## 边界、错误与状态转换

| 场景 | 预期行为 | 关联 ID |
|---|---|---|
| 多日志候选取消 | 旧状态不变 | REQ-002 |
| parser 失败 | service 返回 error，无半成品提交 | REQ-001、REQ-002 |
| 过滤结果为空 | 两明细页均为空，摘要不变 | REQ-002 |
| `macdeployqt` 缺失/失败 | build/install 命令失败并报告，不伪称自包含 | REQ-004 |
| 图标资源缺失或 Info.plist 未声明 | bundle 交付验证失败，不把默认系统图标视为完成 | REQ-004 |
| UI 图标资源无法加载 | GUI 回归失败，不回退为平台原生组合框残片 | REQ-002 |
| 非 macOS host | 不执行 macOS deployment；平台状态未验证 | REQ-004 |

## 约束、假设与风险

### 已确认约束

- 只做近似功能保持的架构重构（FACT-001）。
- 现有 core 行为有测试和旧 Spec 证据，不能盲目重写（FACT-002）。
- required 原生环境是当前 macOS arm64 双 Qt Kit（FACT-005）。

### 待验证假设

- ASM-001：Qt6/Qt5 `macdeployqt` 都能在 install-time 处理含空格的 bundle 路径；实施时必须分别验证。

### 风险

- RISK-001：拆分 Widgets 后信号连接或 parent ownership 回归；用 offscreen GUI 测试覆盖。
- RISK-002：部署工具可能修改签名；本轮只验证本地自包含，不承诺分发签名。
- RISK-003：Windows/Linux 未原生验证，README 必须明确。

## 需求分析记录

| ID | 类型 | 涉及需求 | 发现 | 决议/接受风险 |
|---|---|---|---|---|
| ANA-001 | 歧义 | REQ-001/002 | “功能差不多”可能允许删功能，但无具体删除项 | 以旧自动化和 demo 可观察行为为最低保持线 |
| ANA-002 | 缺口 | REQ-004 | 用户未指定安装包/签名 | 只交付自包含 `.app`，不猜测 DMG/公证 |
| ANA-003 | 约束 | REQ-004 | 最低 macOS 未明示 | 采用已验证两套 Qt framework 的共同下界 11.0，避免旧 app 偶然锁到当前 15.7 |
| ANA-004 | 缺口 | REQ-004 | Windows/Linux 无 runner | 保持源码设计，明确未验证，不阻塞 macOS required |
| ANA-005 | 规格漂移 | REQ-004 | 原 AC-004.1 接受 `build/bin`，但用户明确要求 build 根目录，导致“bundle 已生成”与用户检查路径不一致 | 保留 AC ID，修正为 build 根级唯一 bundle；Windows/Linux 的 bin 约定不变；新增 PROP-006 和 TASK-006 |
| ANA-006 | 规格漂移 | REQ-004 | 用户进一步明确检查的是 `build/bin`，且要求该处是“完整 mac 包”；此前只把完整依赖部署到 stage | 以最新明确要求为准：保留 AC ID，恢复 `build/bin` 精确路径，并把自包含检查前移到默认 build；新增 TASK-007 |
| ANA-007 | 交付缺口 | REQ-004 | bundle 结构、依赖和启动已通过，但没有应用图标资源，因此 Finder 仍显示默认图标 | 在 app 构建单元新增 `.icns` 资源、Info.plist 契约和原生验证；新增 PROP-007 / TASK-008 |
| ANA-008 | 视觉回归 | REQ-002 | 全局 QSS 只覆盖 `QComboBox` 外框和 `QTabBar::tab` 文本/下划线，macOS 原生子控件仍参与绘制 | 为结果标签栏增加专用对象名；由 presentation 模块拥有 SVG 下拉箭头和完整 QSS；新增 PROP-008 / TASK-009 |

## 需求追踪

| 需求 | 验收标准 | 成功证据 |
|---|---|---|
| REQ-001 | AC-001.1—3 | application/core tests、demo |
| REQ-002 | AC-002.1—5 | offscreen GUI tests、Qt5/Qt6 视觉快照 |
| REQ-003 | AC-003.1—4 | target build、inspect、CMake审查 |
| REQ-004 | AC-004.1—5 | bundle/install/deploy/verify_delivery/start smoke |

## 未决问题

- 当前无阻塞设计或实施的未决问题。
