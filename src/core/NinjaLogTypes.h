#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace ninja_analyzer {

enum class StepCategory {
    CCompile,
    CxxCompile,
    CudaCompile,
    QtAutogen,
    Resource,
    StaticLink,
    SharedLink,
    ExecutableLink,
    CustomCommand,
    Other
};

enum class ClassificationSource {
    ManifestRule,
    OutputHeuristic
};

QString categoryDisplayName(StepCategory category);
QString classificationSourceDisplayName(ClassificationSource source);

struct NinjaLogRecord {
    qint64 startMs = 0;
    qint64 endMs = 0;
    qint64 mtime = 0;
    QString output;
    QString commandField;
    int sourceLine = 0;
    bool hasCommandHash = false;
    quint64 commandHash = 0;
    QString rule;
    StepCategory category = StepCategory::Other;
    ClassificationSource classificationSource = ClassificationSource::OutputHeuristic;

    qint64 durationMs() const { return endMs - startMs; }
};

struct ParseWarning {
    int line = 0;
    QString message;
};

struct ParseResult {
    int version = 0;
    QVector<NinjaLogRecord> records;
    QVector<ParseWarning> warnings;
    QString fatalError;

    bool ok() const { return fatalError.isEmpty(); }
};

struct LocateResult {
    QStringList logPaths;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

struct ManifestInfo {
    QString manifestPath;
    QHash<QString, QString> ruleByOutput;
    QStringList warnings;

    bool found() const { return !manifestPath.isEmpty(); }
};

struct BuildBatch {
    int firstRecord = 0;
    int recordCount = 0;
    qint64 minStartMs = 0;
    qint64 maxEndMs = 0;
};

struct SummaryMetrics {
    int taskCount = 0;
    qint64 minStartMs = 0;
    qint64 maxEndMs = 0;
    qint64 observedSpanMs = 0;
    qint64 totalTaskMs = 0;
    double averageParallelism = 0.0;
    int maximumParallelism = 0;
};

struct CategoryStats {
    StepCategory category = StepCategory::Other;
    int count = 0;
    qint64 totalMs = 0;
    double averageMs = 0.0;
    qint64 maximumMs = 0;
    double share = 0.0;
};

enum class InsightSeverity {
    Information,
    Attention,
    Warning
};

struct Insight {
    InsightSeverity severity = InsightSeverity::Information;
    QString title;
    QString detail;
};

struct AnalysisResult {
    SummaryMetrics summary;
    QVector<CategoryStats> categories;
    QVector<NinjaLogRecord> slowest;
    QVector<Insight> insights;
};

} // namespace ninja_analyzer
