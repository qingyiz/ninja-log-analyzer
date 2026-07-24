#include "application/MachineLoadSnapshot.h"

namespace ninja_analyzer {

QString MachineLoadSnapshot::platformDescription() const
{
    QStringList parts;
    if (!kernelType.isEmpty()) {
        parts.append(kernelType);
    }
    if (!kernelVersion.isEmpty()) {
        parts.append(kernelVersion);
    }
    if (!cpuArchitecture.isEmpty()) {
        parts.append(cpuArchitecture);
    }
    return parts.isEmpty() ? QStringLiteral("未知平台") : parts.join(QStringLiteral(" / "));
}

QString MachineLoadSnapshot::loadAverageDescription() const
{
    if (!loadAverageAvailable) {
        return QStringLiteral("系统负载均值不可用");
    }
    return QStringLiteral("1/5/15 分钟负载均值：%1 / %2 / %3")
        .arg(loadAverage1Minute, 0, 'f', 2)
        .arg(loadAverage5Minutes, 0, 'f', 2)
        .arg(loadAverage15Minutes, 0, 'f', 2);
}

QString MachineLoadSnapshot::limitationText()
{
    return QStringLiteral(
        "这是加载日志时分析机器的快照，不是构建时历史负载，也不是 CPU 使用率；"
        "日志可能来自另一台机器。");
}

} // namespace ninja_analyzer
