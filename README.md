# Ninja 构建日志分析器

一个使用 C++17、CMake 和 Qt Widgets 编写的本地桌面工具。它读取 Ninja 的 `.ninja_log`，把难以浏览的文本记录整理成步骤类型、耗时汇总、最慢任务、并发泳道和可解释的瓶颈候选。

## 能看到什么

- 从一个 `.ninja_log` 文件或构建目录开始；目录中有多个日志时会让你选择。
- 解析 Ninja 日志 v4/v5，坏行单独告警，不影响其他有效记录。
- 从邻近 `build.ninja` 补充 rule，区分 C/C++/CUDA 编译、Qt 自动生成、资源、自定义命令和三类链接；匹配不到时回退到输出路径推断。
- 展示任务数、观察窗口、累计任务时间、平均/最大并行度。
- 概览用环形图显示步骤类型耗时占比，用统一尺度的横向条形图显示最慢 5 个任务；图表支持悬停详情和点击下钻。
- 按步骤类型统计任务数、累计/平均/最长耗时和工作量占比。
- 按耗时稳定排序全部任务，并按输出路径或类型过滤。
- 用最多 5000 条最慢任务绘制无重叠泳道时间线，悬停可看精确时间和分类依据。
- 给出累计耗时最高类型、最慢单步、并行分布和尾段长任务候选。

所有分析都在本机内存完成。软件不会运行 Ninja、修改日志、写入构建目录或上传数据。

## 构建

依赖：CMake 3.20+、C++17 编译器、Qt 5.15+ 或 Qt 6（Core、Widgets；测试还需要 Test）。推荐 Ninja 生成器，但不是硬性要求。

以 Qt 6 为例：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Qt 5 只需把 `CMAKE_PREFIX_PATH` 指向 Qt 5 Kit。也可以把 Qt 的 `bin` 目录加入 PATH，让 CMake 自动发现。

macOS 应用位于：

```text
build/Ninja Log Analyzer.app
```

Linux/Windows 的可执行文件在所选构建目录中。源码避免平台专有 UI API；本次交付实际验证的平台是 macOS arm64，Qt 6.4.3 和 Qt 5.15.2。

### Qt 6.4 + 新版 Xcode SDK 的 AGL 说明

本机 Qt 6.4.3 的 CMake `WrapOpenGL` 元数据会向应用额外链接已从 Xcode 26.2 SDK 删除的 AGL。根 `CMakeLists.txt` 只在 Apple + Qt6 + 当前 SDK 确实没有 AGL 时，过滤 imported OpenGL target 中的冗余 AGL 接口项；不会改写 Qt 安装，也不影响 Qt5、其他平台或仍提供 AGL 的 SDK。

## 运行与演示

正常启动后粘贴日志/目录路径，也可以点击“选择日志”或“选择目录”。命令行传入路径会在窗口显示后自动加载：

```bash
"build/Ninja Log Analyzer.app/Contents/MacOS/Ninja Log Analyzer" \
  examples/demo-build
```

仓库自带 [`examples/demo-build`](examples/demo-build) 合成数据，包含两个推断批次、十余种并行任务和对应 `build.ninja`，适合快速查看完整界面。

默认选中最后一个“推断批次”。切换“全部有效记录”可以查看日志中所有行，但它们可能来自不同 Ninja 运行，时间轴只适合做探索，不应当当作一次真实构建。

## 统计口径

- **任务耗时**：`end_ms - start_ms`。
- **观察窗口**：当前范围最早任务开始到最晚任务结束。
- **累计任务时间**：所有任务耗时之和；并行任务会重复计入。
- **平均并行度**：累计任务时间 ÷ 观察窗口。
- **最大并行度**：把任务视为半开区间 `[start, end)` 后的最大同时活动数量。
- **推断批次**：按日志行顺序，当结束时间比上一有效行小时切分。

Ninja 会跨运行追加日志，也可能重整日志；`.ninja_log` 不记录 CPU、内存、磁盘、缓存命中率或完整依赖 DAG。因此本工具显示的是“瓶颈候选”，不是硬件根因或严格关键路径。若要分析一次干净构建，建议保留该次构建单独生成的日志副本，再交给本工具只读分析。

## 项目结构

```text
src/core/   路径发现、日志/manifest 解析、分类与统计（只依赖 Qt Core）
src/gui/    Qt Widgets 主窗口、虚拟慢任务表和 QPainter 时间线
tests/      核心与 offscreen GUI 自动化测试
examples/   可直接加载的合成 Ninja 构建目录
.codex/     requirements/design/tasks 规格与实施证据
```

## 已知边界

- manifest 解析只覆盖 output→rule 所需的常见 Ninja 子集，不是完整 Ninja 解释器；无法识别时会回退分类。
- 推断批次在日志被 `ninja -t recompact` 等操作重整后可能失真。
- 时间线超过 5000 条时只绘制最慢的 5000 条，但摘要、分类和慢任务表仍使用完整数据。
- 本轮不包含代码签名、公证或安装包制作。
