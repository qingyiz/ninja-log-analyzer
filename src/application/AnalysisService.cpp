#include "application/AnalysisService.h"

#include "application/MachineLoadProbe.h"
#include "core/BuildAnalyzer.h"
#include "core/LogLocator.h"
#include "core/NinjaLogParser.h"
#include "core/NinjaManifestParser.h"

#include <QFileInfo>

#include <utility>

namespace ninja_analyzer {

LocateResult AnalysisService::locateLogs(const QString &inputPath) const
{
    return LogLocator::resolve(inputPath);
}

AnalysisLoadResult AnalysisService::loadLog(const QString &logPath) const
{
    ParseResult parsed = NinjaLogParser::parse(logPath);
    if (!parsed.ok()) {
        return {{}, parsed.fatalError};
    }

    LoadedAnalysis loaded;
    const QFileInfo logInfo(logPath);
    loaded.logPath = logInfo.canonicalFilePath();
    if (loaded.logPath.isEmpty()) {
        loaded.logPath = logInfo.absoluteFilePath();
    }
    loaded.logVersion = parsed.version;
    loaded.parseWarnings = std::move(parsed.warnings);
    loaded.manifest = NinjaManifestParser::loadNear(logPath);
    loaded.records = std::move(parsed.records);
    NinjaManifestParser::enrichRecords(loaded.records, loaded.manifest);
    loaded.batches = BuildAnalyzer::partitionBatches(loaded.records);
    if (loaded.batches.isEmpty()) {
        return {{}, QStringLiteral("日志没有形成可分析的记录批次。")};
    }
    loaded.machineLoad = MachineLoadProbe::capture();

    return {std::move(loaded), {}};
}

QVector<NinjaLogRecord> AnalysisService::recordsForBatch(const LoadedAnalysis &loaded,
                                                         int batchIndex) const
{
    if (batchIndex < 0) {
        return loaded.records;
    }
    if (batchIndex >= loaded.batches.size()) {
        return {};
    }
    return BuildAnalyzer::recordsForBatch(loaded.records, loaded.batches.at(batchIndex));
}

AnalysisResult AnalysisService::analyze(const QVector<NinjaLogRecord> &records) const
{
    return BuildAnalyzer::analyze(records);
}

QString AnalysisService::formatDuration(qint64 milliseconds) const
{
    return BuildAnalyzer::formatDuration(milliseconds);
}

} // namespace ninja_analyzer
