#include "core/LogLocator.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <algorithm>

namespace ninja_analyzer {

namespace {

QString normalizedAbsolutePath(const QFileInfo &info)
{
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(QDir::fromNativeSeparators(
        canonical.isEmpty() ? info.absoluteFilePath() : canonical));
}

} // namespace

LocateResult LogLocator::resolve(const QString &inputPath)
{
    LocateResult result;
    const QString trimmedPath = inputPath.trimmed();
    if (trimmedPath.isEmpty()) {
        result.error = QStringLiteral("请输入 .ninja_log 文件或构建目录路径。");
        return result;
    }

    const QFileInfo inputInfo(trimmedPath);
    if (!inputInfo.exists()) {
        result.error = QStringLiteral("路径不存在：%1").arg(trimmedPath);
        return result;
    }

    if (inputInfo.isFile()) {
        if (inputInfo.fileName() != QStringLiteral(".ninja_log")) {
            result.error = QStringLiteral("文件必须名为 .ninja_log：%1")
                               .arg(inputInfo.absoluteFilePath());
            return result;
        }
        if (!inputInfo.isReadable()) {
            result.error = QStringLiteral("日志文件不可读：%1")
                               .arg(inputInfo.absoluteFilePath());
            return result;
        }
        result.logPaths.append(normalizedAbsolutePath(inputInfo));
        return result;
    }

    if (!inputInfo.isDir()) {
        result.error = QStringLiteral("路径既不是普通文件也不是目录：%1")
                           .arg(inputInfo.absoluteFilePath());
        return result;
    }
    if (!inputInfo.isReadable()) {
        result.error = QStringLiteral("目录不可读：%1").arg(inputInfo.absoluteFilePath());
        return result;
    }

    QDirIterator iterator(inputInfo.absoluteFilePath(),
                          QStringList{QStringLiteral(".ninja_log")},
                          QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        result.logPaths.append(normalizedAbsolutePath(iterator.fileInfo()));
    }

    std::sort(result.logPaths.begin(), result.logPaths.end(), [](const QString &left,
                                                                 const QString &right) {
        return QString::compare(left, right, Qt::CaseSensitive) < 0;
    });
    result.logPaths.removeDuplicates();

    if (result.logPaths.isEmpty()) {
        result.error = QStringLiteral("目录中没有找到 .ninja_log：%1")
                           .arg(inputInfo.absoluteFilePath());
    }
    return result;
}

} // namespace ninja_analyzer
