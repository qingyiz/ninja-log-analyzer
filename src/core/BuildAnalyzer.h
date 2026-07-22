#pragma once

#include "core/NinjaLogTypes.h"

#include <QString>

namespace ninja_analyzer {

class BuildAnalyzer final {
public:
    static QVector<BuildBatch> partitionBatches(const QVector<NinjaLogRecord> &records);
    static QVector<NinjaLogRecord> recordsForBatch(const QVector<NinjaLogRecord> &records,
                                                    const BuildBatch &batch);
    static AnalysisResult analyze(const QVector<NinjaLogRecord> &records);
    static QString formatDuration(qint64 milliseconds);
};

} // namespace ninja_analyzer
