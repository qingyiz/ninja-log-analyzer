#include "application/AnalysisReportExporter.h"

#include "core/BuildAnalyzer.h"

#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

namespace ninja_analyzer {

namespace {

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

QString decimal(double value, int precision = 2)
{
    return QString::number(value, 'f', precision);
}

QString severityName(InsightSeverity severity)
{
    switch (severity) {
    case InsightSeverity::Information:
        return QStringLiteral("信息");
    case InsightSeverity::Attention:
        return QStringLiteral("关注");
    case InsightSeverity::Warning:
        return QStringLiteral("警告");
    }
    return QStringLiteral("信息");
}

QString processorDescription(int count)
{
    return count > 0 ? QStringLiteral("%1 个逻辑处理器").arg(count)
                     : QStringLiteral("逻辑处理器数不可用");
}

} // namespace

AnalysisReportResult AnalysisReportExporter::exportHtml(
    const AnalysisReportRequest &request)
{
    if (request.outputPath.trimmed().isEmpty()) {
        return {{}, QStringLiteral("报告保存路径不能为空。")};
    }
    if (request.loaded.logPath.isEmpty() || request.analysis.summary.taskCount <= 0
        || request.analysis.slowest.size() != request.analysis.summary.taskCount) {
        return {{}, QStringLiteral("当前没有可导出的完整分析结果。")};
    }

    QSaveFile file(request.outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {{}, QStringLiteral("无法创建报告 %1：%2")
                        .arg(request.outputPath, file.errorString())};
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif

    const SummaryMetrics &summary = request.analysis.summary;
    const MachineLoadSnapshot &load = request.loaded.machineLoad;
    const QString batchLabel = request.batchIndex < 0
        ? QStringLiteral("全部日志记录")
        : QStringLiteral("推断批次 %1 / %2")
              .arg(request.batchIndex + 1)
              .arg(request.loaded.batches.size());
    const QString manifestStatus = request.loaded.manifest.found()
        ? QStringLiteral("已加载：%1").arg(request.loaded.manifest.manifestPath)
        : QStringLiteral("未找到，分类采用输出路径推断");

    stream << QStringLiteral(
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Ninja 构建完整分析报告</title><style>"
        ":root{color-scheme:light;--ink:#18202b;--muted:#667085;--line:#dce3ec;"
        "--accent:#2563eb;--panel:#fff;--bg:#f4f7fb}"
        "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);"
        "font:14px/1.55 -apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif}"
        "main{max-width:1180px;margin:0 auto;padding:32px 24px 56px}"
        "h1{margin:0 0 6px;font-size:28px}h2{margin:28px 0 12px;font-size:19px}"
        "p{margin:7px 0}.muted{color:var(--muted)}.panel{background:var(--panel);"
        "border:1px solid var(--line);border-radius:12px;padding:18px;margin-top:14px}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}"
        ".metric{border:1px solid var(--line);border-radius:9px;padding:13px}"
        ".metric b{display:block;font-size:21px;margin-top:4px}.note{border-left:4px solid "
        "var(--accent);padding:10px 13px;background:#eff6ff;border-radius:4px}"
        "table{width:100%;border-collapse:collapse;background:#fff}th,td{padding:9px 10px;"
        "border:1px solid var(--line);text-align:left;vertical-align:top}th{background:#eef3f9;"
        "position:sticky;top:0}td.num{text-align:right;white-space:nowrap}.scroll{overflow:auto}"
        "code{font:12px ui-monospace,SFMono-Regular,Menlo,monospace;word-break:break-all}"
        "ul{padding-left:22px}.tag{display:inline-block;border-radius:999px;background:#eef2ff;"
        "padding:2px 8px;margin-right:6px}</style></head><body><main>");
    stream << QStringLiteral("<h1>Ninja 构建完整分析报告</h1><p class=\"muted\">生成时间：%1</p>")
                  .arg(escaped(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)));

    stream << QStringLiteral(
        "<section class=\"panel\"><h2>日志与分析范围</h2>"
        "<p><b>源文件：</b><code>%1</code></p>"
        "<p><b>格式：</b>Ninja log v%2</p>"
        "<p><b>范围：</b>%3；相对时间 %4–%5 ms</p>"
        "<p><b>解析：</b>%6 个有效任务，忽略 %7 行，共推断 %8 个批次</p>"
        "<p><b>Manifest：</b>%9</p></section>")
                  .arg(escaped(request.loaded.logPath))
                  .arg(request.loaded.logVersion)
                  .arg(escaped(batchLabel))
                  .arg(summary.minStartMs)
                  .arg(summary.maxEndMs)
                  .arg(request.loaded.records.size())
                  .arg(request.loaded.parseWarnings.size())
                  .arg(request.loaded.batches.size())
                  .arg(escaped(manifestStatus));

    stream << QStringLiteral(
        "<h2>五项摘要指标</h2><section class=\"grid\">"
        "<div class=\"metric\">任务数<b>%1</b></div>"
        "<div class=\"metric\">观察窗口<b>%2</b></div>"
        "<div class=\"metric\">累计任务时间<b>%3</b></div>"
        "<div class=\"metric\">平均并行度<b>%4</b></div>"
        "<div class=\"metric\">峰值并行度<b>%5</b></div></section>")
                  .arg(summary.taskCount)
                  .arg(escaped(BuildAnalyzer::formatDuration(summary.observedSpanMs)))
                  .arg(escaped(BuildAnalyzer::formatDuration(summary.totalTaskMs)))
                  .arg(decimal(summary.averageParallelism))
                  .arg(summary.maximumParallelism);

    stream << QStringLiteral(
        "<section class=\"panel\"><h2>并发时间线与泳道怎么理解</h2>"
        "<p class=\"note\">泳道是为了让时间区间互不遮挡而分配的显示行，数量等于这批记录"
        "所需的最少不重叠行数；它不是 CPU 核心数、线程数或 Ninja worker 数。</p>"
        "<p>例如机器有 8 个逻辑处理器而时间线出现 24 条泳道，说明日志中最多需要同时摆放"
        "24 个重叠区间。可能与 Ninja <code>-j</code> 并行度、I/O 等待、逻辑处理器、任务"
        "区间重叠或批次推断有关；仅凭 Ninja 日志不能证明精确原因。</p></section>");

    stream << QStringLiteral(
        "<section class=\"panel\"><h2>分析机器负载快照</h2>"
        "<p><b>采集时间：</b>%1</p><p><b>平台：</b>%2</p>"
        "<p><b>处理器：</b>%3</p><p><b>负载：</b>%4</p>"
        "<p class=\"note\">%5</p></section>")
                  .arg(escaped(load.capturedAtUtc.isValid()
                                   ? load.capturedAtUtc.toString(Qt::ISODate)
                                   : QStringLiteral("不可用")))
                  .arg(escaped(load.platformDescription()))
                  .arg(escaped(processorDescription(load.logicalProcessorCount)))
                  .arg(escaped(load.loadAverageDescription()))
                  .arg(escaped(MachineLoadSnapshot::limitationText()));

    stream << QStringLiteral(
        "<h2>耗时分类</h2><div class=\"scroll\"><table><thead><tr>"
        "<th>类型</th><th>任务数</th><th>累计耗时</th><th>平均耗时</th>"
        "<th>最大耗时</th><th>占累计时间</th></tr></thead><tbody>");
    for (const CategoryStats &category : request.analysis.categories) {
        stream << QStringLiteral(
            "<tr><td>%1</td><td class=\"num\">%2</td><td class=\"num\">%3</td>"
            "<td class=\"num\">%4</td><td class=\"num\">%5</td><td class=\"num\">%6%</td></tr>")
                      .arg(escaped(categoryDisplayName(category.category)))
                      .arg(category.count)
                      .arg(escaped(BuildAnalyzer::formatDuration(category.totalMs)))
                      .arg(escaped(BuildAnalyzer::formatDuration(
                          static_cast<qint64>(category.averageMs))))
                      .arg(escaped(BuildAnalyzer::formatDuration(category.maximumMs)))
                      .arg(decimal(category.share * 100.0, 1));
    }
    stream << QStringLiteral("</tbody></table></div>");

    stream << QStringLiteral("<section class=\"panel\"><h2>分析洞察</h2><ul>");
    if (request.analysis.insights.isEmpty()) {
        stream << QStringLiteral("<li>当前范围没有生成额外洞察。</li>");
    } else {
        for (const Insight &insight : request.analysis.insights) {
            stream << QStringLiteral("<li><span class=\"tag\">%1</span><b>%2：</b>%3</li>")
                          .arg(escaped(severityName(insight.severity)),
                               escaped(insight.title),
                               escaped(insight.detail));
        }
    }
    stream << QStringLiteral("</ul></section>");

    stream << QStringLiteral(
        "<h2>当前批次全部任务（%1）</h2><p class=\"muted\">按耗时从慢到快排序；"
        "本表不受界面类型或搜索筛选影响。</p><div class=\"scroll\"><table>"
        "<thead><tr><th>#</th><th>输出</th><th>类型</th><th>耗时</th><th>开始</th>"
        "<th>结束</th><th>分类依据</th><th>规则/命令哈希</th></tr></thead><tbody>")
                  .arg(request.analysis.slowest.size());
    int row = 0;
    for (const NinjaLogRecord &record : request.analysis.slowest) {
        ++row;
        QString ruleOrHash = record.rule;
        if (ruleOrHash.isEmpty() && record.hasCommandHash) {
            ruleOrHash = QStringLiteral("0x%1").arg(record.commandHash, 0, 16);
        }
        if (ruleOrHash.isEmpty()) {
            ruleOrHash = QStringLiteral("—");
        }
        stream << QStringLiteral(
            "<tr data-task-row=\"1\"><td class=\"num\">%1</td><td><code>%2</code></td>"
            "<td>%3</td><td class=\"num\">%4</td><td class=\"num\">%5 ms</td>"
            "<td class=\"num\">%6 ms</td><td>%7</td><td><code>%8</code></td></tr>")
                      .arg(row)
                      .arg(escaped(record.output))
                      .arg(escaped(categoryDisplayName(record.category)))
                      .arg(escaped(BuildAnalyzer::formatDuration(record.durationMs())))
                      .arg(record.startMs)
                      .arg(record.endMs)
                      .arg(escaped(classificationSourceDisplayName(
                          record.classificationSource)))
                      .arg(escaped(ruleOrHash));
    }
    stream << QStringLiteral(
        "</tbody></table></div><p class=\"muted\">报告由 Ninja 构建日志分析器生成。"
        "结论仅描述日志中的耗时与重叠，不等同于硬件根因或严格关键路径。</p>"
        "</main></body></html>");
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        file.cancelWriting();
        return {{}, QStringLiteral("写入报告失败：%1").arg(request.outputPath)};
    }
    if (!file.commit()) {
        return {{}, QStringLiteral("提交报告失败 %1：%2")
                        .arg(request.outputPath, file.errorString())};
    }
    return {QFileInfo(request.outputPath).absoluteFilePath(), {}};
}

} // namespace ninja_analyzer
