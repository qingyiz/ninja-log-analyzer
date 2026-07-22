#pragma once

#include "core/NinjaLogTypes.h"

#include <QString>

namespace ninja_analyzer {

class NinjaManifestParser final {
public:
    static ManifestInfo loadNear(const QString &logPath);
    static void enrichRecords(QVector<NinjaLogRecord> &records,
                              const ManifestInfo &manifest);

    static StepCategory categoryFromRule(const QString &rule);
    static StepCategory categoryFromOutput(const QString &output);
};

} // namespace ninja_analyzer
