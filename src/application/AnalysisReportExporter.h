#pragma once

#include "application/AnalysisService.h"

#include <QString>

namespace ninja_analyzer {

struct AnalysisReportRequest {
    QString outputPath;
    LoadedAnalysis loaded;
    int batchIndex = -1;
    AnalysisResult analysis;
};

struct AnalysisReportResult {
    QString outputPath;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

class AnalysisReportExporter final {
public:
    static AnalysisReportResult exportHtml(const AnalysisReportRequest &request);
};

} // namespace ninja_analyzer
