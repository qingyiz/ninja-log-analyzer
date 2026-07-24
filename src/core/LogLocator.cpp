#include "core/LogLocator.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace ninja_analyzer {

namespace {

QString normalizedAbsolutePath(const QFileInfo &info)
{
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(QDir::fromNativeSeparators(
        canonical.isEmpty() ? info.absoluteFilePath() : canonical));
}

bool hasNinjaLogSignature(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    constexpr qint64 kProbeBytes = 128;
    QByteArray prefix = file.read(kProbeBytes);
    const int lineEnd = prefix.indexOf('\n');
    if (lineEnd >= 0) {
        prefix.truncate(lineEnd);
    }
    if (prefix.endsWith('\r')) {
        prefix.chop(1);
    }

    static const QRegularExpression signature(
        QStringLiteral("^# ninja log v[0-9]+$"));
    return signature.match(QString::fromLatin1(prefix)).hasMatch();
}

} // namespace

LocateResult LogLocator::resolve(const QString &inputPath)
{
    LocateResult result;
    const QString trimmedPath = inputPath.trimmed();
    if (trimmedPath.isEmpty()) {
        result.error = QStringLiteral("请输入 Ninja 日志文件或构建目录路径。");
        return result;
    }

    const QFileInfo inputInfo(trimmedPath);
    if (!inputInfo.exists()) {
        result.error = QStringLiteral("路径不存在：%1").arg(trimmedPath);
        return result;
    }

    if (inputInfo.isFile()) {
        if (!inputInfo.isReadable()) {
            result.error = QStringLiteral("日志文件不可读：%1")
                               .arg(inputInfo.absoluteFilePath());
            return result;
        }
        if (!hasNinjaLogSignature(inputInfo.absoluteFilePath())) {
            result.error = QStringLiteral("文件内容不是 Ninja 日志：%1")
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
                          QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        if (hasNinjaLogSignature(iterator.filePath())) {
            result.logPaths.append(normalizedAbsolutePath(iterator.fileInfo()));
        }
    }

    std::sort(result.logPaths.begin(), result.logPaths.end(), [](const QString &left,
                                                                 const QString &right) {
        return QString::compare(left, right, Qt::CaseSensitive) < 0;
    });
    result.logPaths.removeDuplicates();

    if (result.logPaths.isEmpty()) {
        result.error = QStringLiteral("目录中没有找到内容有效的 Ninja 日志：%1")
                           .arg(inputInfo.absoluteFilePath());
    }
    return result;
}

} // namespace ninja_analyzer
