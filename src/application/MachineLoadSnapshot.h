#pragma once

#include <QDateTime>
#include <QString>

namespace ninja_analyzer {

struct MachineLoadSnapshot {
    QDateTime capturedAtUtc;
    int logicalProcessorCount = -1;
    QString kernelType;
    QString kernelVersion;
    QString cpuArchitecture;
    bool loadAverageAvailable = false;
    double loadAverage1Minute = 0.0;
    double loadAverage5Minutes = 0.0;
    double loadAverage15Minutes = 0.0;

    QString platformDescription() const;
    QString loadAverageDescription() const;
    static QString limitationText();
};

} // namespace ninja_analyzer
