#include "core/NinjaLogParser.h"

#include <QFile>
#include <QRegularExpression>

namespace ninja_analyzer {

namespace {

void stripLineEnding(QByteArray &line)
{
    if (line.endsWith('\n')) {
        line.chop(1);
    }
    if (line.endsWith('\r')) {
        line.chop(1);
    }
}

void addWarning(ParseResult &result, int line, const QString &message)
{
    result.warnings.append(ParseWarning{line, message});
}

bool isHexField(const QByteArray &field)
{
    if (field.isEmpty()) {
        return false;
    }
    for (const char character : field) {
        const bool decimal = character >= '0' && character <= '9';
        const bool lower = character >= 'a' && character <= 'f';
        const bool upper = character >= 'A' && character <= 'F';
        if (!decimal && !lower && !upper) {
            return false;
        }
    }
    return true;
}

} // namespace

ParseResult NinjaLogParser::parse(const QString &logPath)
{
    ParseResult result;
    QFile file(logPath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.fatalError = QStringLiteral("无法读取日志 %1：%2")
                                .arg(logPath, file.errorString());
        return result;
    }

    if (file.atEnd()) {
        result.fatalError = QStringLiteral("日志为空：%1").arg(logPath);
        return result;
    }

    QByteArray header = file.readLine();
    stripLineEnding(header);
    static const QRegularExpression signature(
        QStringLiteral("^# ninja log v([0-9]+)$"));
    const QRegularExpressionMatch match = signature.match(QString::fromLatin1(header));
    if (!match.hasMatch()) {
        result.fatalError = QStringLiteral("缺少有效的 Ninja 日志签名：%1")
                                .arg(QString::fromUtf8(header));
        return result;
    }

    bool versionOk = false;
    result.version = match.captured(1).toInt(&versionOk);
    if (!versionOk || (result.version != 4 && result.version != 5)) {
        result.fatalError = QStringLiteral("不支持 Ninja 日志 v%1；当前仅支持 v4 和 v5。")
                                .arg(match.captured(1));
        return result;
    }

    int sourceLine = 1;
    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        ++sourceLine;
        stripLineEnding(line);

        int separators[4] = {-1, -1, -1, -1};
        int searchFrom = 0;
        bool hasAllSeparators = true;
        for (int index = 0; index < 4; ++index) {
            separators[index] = line.indexOf('\t', searchFrom);
            if (separators[index] < 0) {
                hasAllSeparators = false;
                break;
            }
            searchFrom = separators[index] + 1;
        }
        if (!hasAllSeparators) {
            addWarning(result, sourceLine, QStringLiteral("字段不足，已忽略该行。"));
            continue;
        }

        const QByteArray startField = line.left(separators[0]);
        const QByteArray endField = line.mid(separators[0] + 1,
                                             separators[1] - separators[0] - 1);
        const QByteArray mtimeField = line.mid(separators[1] + 1,
                                               separators[2] - separators[1] - 1);
        const QByteArray outputField = line.mid(separators[2] + 1,
                                                separators[3] - separators[2] - 1);
        const QByteArray commandField = line.mid(separators[3] + 1);

        bool startOk = false;
        bool endOk = false;
        bool mtimeOk = false;
        const qint64 startMs = startField.toLongLong(&startOk, 10);
        const qint64 endMs = endField.toLongLong(&endOk, 10);
        const qint64 mtime = mtimeField.toLongLong(&mtimeOk, 10);
        if (!startOk || !endOk || !mtimeOk) {
            addWarning(result, sourceLine, QStringLiteral("时间字段不是有效整数，已忽略该行。"));
            continue;
        }
        if (startMs < 0 || endMs < 0 || endMs < startMs) {
            addWarning(result, sourceLine, QStringLiteral("开始/结束时间范围无效，已忽略该行。"));
            continue;
        }
        if (outputField.isEmpty()) {
            addWarning(result, sourceLine, QStringLiteral("输出路径为空，已忽略该行。"));
            continue;
        }

        NinjaLogRecord record;
        record.startMs = startMs;
        record.endMs = endMs;
        record.mtime = mtime;
        record.output = QString::fromUtf8(outputField);
        record.commandField = QString::fromUtf8(commandField);
        record.sourceLine = sourceLine;

        if (result.version == 5) {
            bool hashOk = false;
            if (!isHexField(commandField)) {
                addWarning(result, sourceLine,
                           QStringLiteral("v5 命令哈希不是十六进制，已忽略该行。"));
                continue;
            }
            record.commandHash = commandField.toULongLong(&hashOk, 16);
            if (!hashOk) {
                addWarning(result, sourceLine,
                           QStringLiteral("v5 命令哈希超出 64 位范围，已忽略该行。"));
                continue;
            }
            record.hasV5Hash = true;
        }

        result.records.append(record);
    }

    if (result.records.isEmpty()) {
        result.fatalError = QStringLiteral("日志中没有可分析的有效记录（忽略 %1 行）。")
                                .arg(result.warnings.size());
    }
    return result;
}

} // namespace ninja_analyzer
