#include "core/BuildAnalyzer.h"

#include <QMap>

#include <algorithm>
#include <cmath>

namespace ninja_analyzer {

namespace {

struct ConcurrencyEvent {
    qint64 time = 0;
    int delta = 0;
};

QString formatDecimal(double value, int decimals)
{
    QString formatted = QString::number(value, 'f', decimals);
    while (formatted.contains(QLatin1Char('.')) && formatted.endsWith(QLatin1Char('0'))) {
        formatted.chop(1);
    }
    if (formatted.endsWith(QLatin1Char('.'))) {
        formatted.chop(1);
    }
    return formatted;
}

QVector<Insight> buildInsights(const AnalysisResult &analysis)
{
    QVector<Insight> insights;
    if (analysis.summary.taskCount == 0) {
        return insights;
    }

    if (!analysis.categories.isEmpty()) {
        const CategoryStats &top = analysis.categories.first();
        insights.append(Insight{
            InsightSeverity::Attention,
            QStringLiteral("累计耗时最高类型"),
            QStringLiteral("%1累计 %2，占全部任务时间 %3%。")
                .arg(categoryDisplayName(top.category),
                     BuildAnalyzer::formatDuration(top.totalMs),
                     formatDecimal(top.share * 100.0, 1))});
    }

    if (!analysis.slowest.isEmpty()) {
        const NinjaLogRecord &slowest = analysis.slowest.first();
        QString shareText;
        if (analysis.summary.observedSpanMs > 0) {
            const double share = static_cast<double>(slowest.durationMs())
                                 / analysis.summary.observedSpanMs * 100.0;
            shareText = QStringLiteral("，相当于观察窗口的 %1%")
                            .arg(formatDecimal(share, 1));
        }
        insights.append(Insight{
            InsightSeverity::Warning,
            QStringLiteral("最慢单步"),
            QStringLiteral("%1耗时 %2%3。")
                .arg(slowest.output, BuildAnalyzer::formatDuration(slowest.durationMs()), shareText)});
    }

    const SummaryMetrics &summary = analysis.summary;
    if (summary.maximumParallelism >= 2
        && summary.averageParallelism < summary.maximumParallelism * 0.5) {
        insights.append(Insight{
            InsightSeverity::Attention,
            QStringLiteral("并行分布不均候选"),
            QStringLiteral("平均并行度 %1，仅为峰值 %2 的 %3%；可检查长尾、依赖等待或并行任务供给。")
                .arg(formatDecimal(summary.averageParallelism, 2))
                .arg(summary.maximumParallelism)
                .arg(formatDecimal(summary.averageParallelism
                                       / summary.maximumParallelism * 100.0,
                                   1))});
    } else if (summary.taskCount >= 4 && summary.averageParallelism < 1.5) {
        insights.append(Insight{
            InsightSeverity::Attention,
            QStringLiteral("整体串行候选"),
            QStringLiteral("平均并行度为 %1；日志只能证明任务时间重叠较少，不能单独证明 CPU 利用率。")
                .arg(formatDecimal(summary.averageParallelism, 2))});
    }

    QVector<qint64> durations;
    durations.reserve(analysis.slowest.size());
    for (const NinjaLogRecord &record : analysis.slowest) {
        durations.append(qMax<qint64>(0, record.durationMs()));
    }
    std::sort(durations.begin(), durations.end());
    const int percentileIndex = qMax(0, static_cast<int>(std::ceil(durations.size() * 0.9)) - 1);
    const qint64 threshold = qMax<qint64>(1000, durations.at(percentileIndex));
    const qint64 tailStart = summary.minStartMs
                             + static_cast<qint64>(summary.observedSpanMs * 0.9);

    QStringList tailOutputs;
    for (const NinjaLogRecord &record : analysis.slowest) {
        if (record.endMs >= tailStart && record.durationMs() >= threshold) {
            tailOutputs.append(QStringLiteral("%1（%2）")
                                   .arg(record.output,
                                        BuildAnalyzer::formatDuration(record.durationMs())));
            if (tailOutputs.size() == 3) {
                break;
            }
        }
    }
    if (!tailOutputs.isEmpty()) {
        insights.append(Insight{
            InsightSeverity::Warning,
            QStringLiteral("尾段长任务候选"),
            QStringLiteral("这些任务在观察窗口最后 10% 结束，且耗时不低于 max(1 s, P90)：%1。")
                .arg(tailOutputs.join(QStringLiteral("；")))});
    }

    return insights;
}

} // namespace

QVector<BuildBatch> BuildAnalyzer::partitionBatches(const QVector<NinjaLogRecord> &records)
{
    QVector<BuildBatch> batches;
    if (records.isEmpty()) {
        return batches;
    }

    int batchStart = 0;
    qint64 minStart = records.first().startMs;
    qint64 maxEnd = records.first().endMs;
    for (int index = 1; index < records.size(); ++index) {
        const NinjaLogRecord &record = records.at(index);
        if (record.endMs < records.at(index - 1).endMs) {
            batches.append(BuildBatch{batchStart, index - batchStart, minStart, maxEnd});
            batchStart = index;
            minStart = record.startMs;
            maxEnd = record.endMs;
        } else {
            minStart = qMin(minStart, record.startMs);
            maxEnd = qMax(maxEnd, record.endMs);
        }
    }
    batches.append(BuildBatch{batchStart,
                              static_cast<int>(records.size()) - batchStart,
                              minStart,
                              maxEnd});
    return batches;
}

QVector<NinjaLogRecord> BuildAnalyzer::recordsForBatch(
    const QVector<NinjaLogRecord> &records,
    const BuildBatch &batch)
{
    if (batch.firstRecord < 0 || batch.recordCount <= 0
        || batch.firstRecord >= records.size()) {
        return {};
    }
    const int count = qMin(batch.recordCount, records.size() - batch.firstRecord);
    QVector<NinjaLogRecord> selected;
    selected.reserve(count);
    for (int index = 0; index < count; ++index) {
        selected.append(records.at(batch.firstRecord + index));
    }
    return selected;
}

AnalysisResult BuildAnalyzer::analyze(const QVector<NinjaLogRecord> &records)
{
    AnalysisResult result;
    if (records.isEmpty()) {
        return result;
    }

    SummaryMetrics &summary = result.summary;
    summary.taskCount = records.size();
    summary.minStartMs = records.first().startMs;
    summary.maxEndMs = records.first().endMs;

    QMap<int, CategoryStats> categoryMap;
    QVector<ConcurrencyEvent> events;
    events.reserve(records.size() * 2);
    for (const NinjaLogRecord &record : records) {
        summary.minStartMs = qMin(summary.minStartMs, record.startMs);
        summary.maxEndMs = qMax(summary.maxEndMs, record.endMs);
        const qint64 duration = qMax<qint64>(0, record.durationMs());
        summary.totalTaskMs += duration;

        const int categoryKey = static_cast<int>(record.category);
        CategoryStats stats = categoryMap.value(categoryKey);
        stats.category = record.category;
        ++stats.count;
        stats.totalMs += duration;
        stats.maximumMs = qMax(stats.maximumMs, duration);
        categoryMap.insert(categoryKey, stats);

        if (duration > 0) {
            events.append(ConcurrencyEvent{record.startMs, +1});
            events.append(ConcurrencyEvent{record.endMs, -1});
        }
    }

    summary.observedSpanMs = qMax<qint64>(0, summary.maxEndMs - summary.minStartMs);
    if (summary.observedSpanMs > 0) {
        summary.averageParallelism = static_cast<double>(summary.totalTaskMs)
                                     / summary.observedSpanMs;
    }

    std::sort(events.begin(), events.end(), [](const ConcurrencyEvent &left,
                                               const ConcurrencyEvent &right) {
        if (left.time != right.time) {
            return left.time < right.time;
        }
        return left.delta < right.delta;
    });
    int active = 0;
    for (const ConcurrencyEvent &event : events) {
        active = qMax(0, active + event.delta);
        summary.maximumParallelism = qMax(summary.maximumParallelism, active);
    }

    result.categories.reserve(categoryMap.size());
    for (auto iterator = categoryMap.constBegin(); iterator != categoryMap.constEnd(); ++iterator) {
        CategoryStats stats = iterator.value();
        stats.averageMs = stats.count > 0
                              ? static_cast<double>(stats.totalMs) / stats.count
                              : 0.0;
        stats.share = summary.totalTaskMs > 0
                          ? static_cast<double>(stats.totalMs) / summary.totalTaskMs
                          : 0.0;
        result.categories.append(stats);
    }
    std::sort(result.categories.begin(), result.categories.end(),
              [](const CategoryStats &left, const CategoryStats &right) {
        if (left.totalMs != right.totalMs) {
            return left.totalMs > right.totalMs;
        }
        return categoryDisplayName(left.category) < categoryDisplayName(right.category);
    });

    result.slowest = records;
    std::sort(result.slowest.begin(), result.slowest.end(),
              [](const NinjaLogRecord &left, const NinjaLogRecord &right) {
        if (left.durationMs() != right.durationMs()) {
            return left.durationMs() > right.durationMs();
        }
        const int outputOrder = QString::compare(left.output, right.output, Qt::CaseSensitive);
        if (outputOrder != 0) {
            return outputOrder < 0;
        }
        return left.sourceLine < right.sourceLine;
    });

    result.insights = buildInsights(result);
    return result;
}

QString BuildAnalyzer::formatDuration(qint64 milliseconds)
{
    milliseconds = qMax<qint64>(0, milliseconds);
    if (milliseconds < 1000) {
        return QStringLiteral("%1 ms").arg(milliseconds);
    }
    if (milliseconds < 60000) {
        const int decimals = milliseconds < 10000 ? 2 : 1;
        return QStringLiteral("%1 s")
            .arg(formatDecimal(static_cast<double>(milliseconds) / 1000.0, decimals));
    }

    const qint64 minutes = milliseconds / 60000;
    const qint64 remainder = milliseconds % 60000;
    if (remainder == 0) {
        return QStringLiteral("%1 min").arg(minutes);
    }
    return QStringLiteral("%1 min %2 s")
        .arg(minutes)
        .arg(formatDecimal(static_cast<double>(remainder) / 1000.0, 1));
}

} // namespace ninja_analyzer
