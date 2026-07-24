#include "application/MachineLoadProbe.h"

#include <QSysInfo>
#include <QThread>

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
#include <cstdlib>
#endif

namespace ninja_analyzer {

MachineLoadSnapshot MachineLoadProbe::capture()
{
    MachineLoadSnapshot snapshot;
    snapshot.capturedAtUtc = QDateTime::currentDateTimeUtc();
    snapshot.logicalProcessorCount = QThread::idealThreadCount();
    snapshot.kernelType = QSysInfo::kernelType();
    snapshot.kernelVersion = QSysInfo::kernelVersion();
    snapshot.cpuArchitecture = QSysInfo::currentCpuArchitecture();

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    double averages[3] = {0.0, 0.0, 0.0};
    if (::getloadavg(averages, 3) == 3) {
        snapshot.loadAverageAvailable = true;
        snapshot.loadAverage1Minute = averages[0];
        snapshot.loadAverage5Minutes = averages[1];
        snapshot.loadAverage15Minutes = averages[2];
    }
#endif

    return snapshot;
}

} // namespace ninja_analyzer
