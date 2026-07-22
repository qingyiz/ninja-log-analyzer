#pragma once

#include "core/NinjaLogTypes.h"

#include <QString>

namespace ninja_analyzer {

struct LoadedAnalysis {
    QString logPath;
    int logVersion = 0;
    QVector<ParseWarning> parseWarnings;
    ManifestInfo manifest;
    QVector<NinjaLogRecord> records;
    QVector<BuildBatch> batches;
};

struct AnalysisLoadResult {
    LoadedAnalysis value;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

class AnalysisService final {
public:
    LocateResult locateLogs(const QString &inputPath) const;
    AnalysisLoadResult loadLog(const QString &logPath) const;
    QVector<NinjaLogRecord> recordsForBatch(const LoadedAnalysis &loaded,
                                            int batchIndex) const;
    AnalysisResult analyze(const QVector<NinjaLogRecord> &records) const;
    QString formatDuration(qint64 milliseconds) const;
};

} // namespace ninja_analyzer
