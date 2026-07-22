#include "core/NinjaManifestParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace ninja_analyzer {

namespace {

constexpr int kMaximumManifestFiles = 32;

QString normalizedPath(QString path)
{
    path = QDir::cleanPath(QDir::fromNativeSeparators(path));
    while (path.startsWith(QStringLiteral("./"))) {
        path.remove(0, 2);
    }
#ifdef Q_OS_WIN
    path = path.toLower();
#endif
    return path;
}

QStringList tokenizeNinja(const QString &text)
{
    QStringList tokens;
    QString token;
    for (int index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (character == QLatin1Char('$') && index + 1 < text.size()) {
            const QChar escaped = text.at(index + 1);
            if (escaped == QLatin1Char(' ') || escaped == QLatin1Char(':')
                || escaped == QLatin1Char('$')) {
                token.append(escaped);
                ++index;
                continue;
            }
            token.append(character);
            continue;
        }
        if (character.isSpace()) {
            if (!token.isEmpty()) {
                tokens.append(token);
                token.clear();
            }
            continue;
        }
        token.append(character);
    }
    if (!token.isEmpty()) {
        tokens.append(token);
    }
    return tokens;
}

int findUnescapedColon(const QString &text)
{
    for (int index = 0; index < text.size(); ++index) {
        if (text.at(index) == QLatin1Char('$') && index + 1 < text.size()) {
            ++index;
            continue;
        }
        if (text.at(index) == QLatin1Char(':')) {
            return index;
        }
    }
    return -1;
}

bool hasUnescapedContinuation(const QString &line)
{
    int dollarCount = 0;
    for (int index = line.size() - 1;
         index >= 0 && line.at(index) == QLatin1Char('$');
         --index) {
        ++dollarCount;
    }
    return dollarCount % 2 == 1;
}

QStringList readLogicalLines(QFile &file)
{
    QStringList logicalLines;
    QString pending;
    while (!file.atEnd()) {
        QString physical = QString::fromUtf8(file.readLine());
        if (physical.endsWith(QLatin1Char('\n'))) {
            physical.chop(1);
        }
        if (physical.endsWith(QLatin1Char('\r'))) {
            physical.chop(1);
        }

        const bool continued = hasUnescapedContinuation(physical);
        if (continued) {
            physical.chop(1);
        }
        if (pending.isEmpty()) {
            pending = physical;
        } else {
            pending += physical.trimmed();
        }

        if (!continued) {
            logicalLines.append(pending);
            pending.clear();
        }
    }
    if (!pending.isEmpty()) {
        logicalLines.append(pending);
    }
    return logicalLines;
}

class ManifestLoader final {
public:
    explicit ManifestLoader(const QString &rootManifest)
        : rootDirectory_(QFileInfo(rootManifest).absolutePath())
    {
    }

    ManifestInfo load(const QString &rootManifest)
    {
        ManifestInfo result;
        result.manifestPath = QFileInfo(rootManifest).canonicalFilePath();
        if (result.manifestPath.isEmpty()) {
            result.manifestPath = QFileInfo(rootManifest).absoluteFilePath();
        }
        parseFile(result.manifestPath, result);
        return result;
    }

private:
    void parseFile(const QString &path, ManifestInfo &result)
    {
        if (visited_.size() >= kMaximumManifestFiles) {
            result.warnings.append(
                QStringLiteral("manifest 文件超过 %1 个，后续 include/subninja 已忽略。")
                    .arg(kMaximumManifestFiles));
            return;
        }

        QFileInfo info(path);
        QString canonical = info.canonicalFilePath();
        if (canonical.isEmpty()) {
            canonical = info.absoluteFilePath();
        }
        if (visited_.contains(canonical)) {
            return;
        }
        visited_.insert(canonical);

        QFile file(canonical);
        if (!file.open(QIODevice::ReadOnly)) {
            result.warnings.append(QStringLiteral("无法读取 manifest %1：%2")
                                       .arg(canonical, file.errorString()));
            return;
        }

        const QStringList lines = readLogicalLines(file);
        for (const QString &originalLine : lines) {
            const QString line = originalLine.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            if (line.startsWith(QStringLiteral("include "))
                || line.startsWith(QStringLiteral("subninja "))) {
                const int separator = line.indexOf(QLatin1Char(' '));
                const QString includeExpression = line.mid(separator + 1).trimmed();
                if (includeExpression.contains(QLatin1Char('$'))) {
                    result.warnings.append(
                        QStringLiteral("无法展开包含变量的 manifest 路径：%1")
                            .arg(includeExpression));
                    continue;
                }
                const QStringList includeTokens = tokenizeNinja(includeExpression);
                if (includeTokens.size() != 1) {
                    result.warnings.append(
                        QStringLiteral("无法识别 manifest 路径：%1").arg(includeExpression));
                    continue;
                }
                parseFile(QDir(info.absolutePath()).filePath(includeTokens.first()), result);
                continue;
            }
            if (!line.startsWith(QStringLiteral("build "))) {
                continue;
            }

            const QString statement = line.mid(6);
            const int colon = findUnescapedColon(statement);
            if (colon < 0) {
                result.warnings.append(QStringLiteral("无法识别 build statement：%1").arg(line));
                continue;
            }

            QStringList outputTokens = tokenizeNinja(statement.left(colon));
            const QStringList rightTokens = tokenizeNinja(statement.mid(colon + 1));
            if (rightTokens.isEmpty()) {
                result.warnings.append(QStringLiteral("build statement 缺少 rule：%1").arg(line));
                continue;
            }
            const QString rule = rightTokens.first();
            for (const QString &output : outputTokens) {
                if (output == QStringLiteral("|") || output == QStringLiteral("||")) {
                    continue;
                }
                const QString relativeKey = normalizedPath(output);
                if (relativeKey.isEmpty()) {
                    continue;
                }
                result.ruleByOutput.insert(relativeKey, rule);
                if (QFileInfo(output).isRelative()) {
                    result.ruleByOutput.insert(
                        normalizedPath(QDir(rootDirectory_).absoluteFilePath(output)), rule);
                }
            }
        }
    }

    QString rootDirectory_;
    QSet<QString> visited_;
};

QString matchedRule(const NinjaLogRecord &record, const ManifestInfo &manifest)
{
    const QString rawKey = normalizedPath(record.output);
    auto iterator = manifest.ruleByOutput.constFind(rawKey);
    if (iterator != manifest.ruleByOutput.constEnd()) {
        return iterator.value();
    }
    if (!manifest.manifestPath.isEmpty() && QFileInfo(record.output).isRelative()) {
        const QString absoluteKey = normalizedPath(
            QDir(QFileInfo(manifest.manifestPath).absolutePath()).absoluteFilePath(record.output));
        iterator = manifest.ruleByOutput.constFind(absoluteKey);
        if (iterator != manifest.ruleByOutput.constEnd()) {
            return iterator.value();
        }
    }
    return {};
}

} // namespace

ManifestInfo NinjaManifestParser::loadNear(const QString &logPath)
{
    QDir directory(QFileInfo(logPath).absolutePath());
    for (int level = 0; level <= 4; ++level) {
        const QString candidate = directory.filePath(QStringLiteral("build.ninja"));
        const QFileInfo candidateInfo(candidate);
        if (candidateInfo.isFile() && candidateInfo.isReadable()) {
            return ManifestLoader(candidate).load(candidate);
        }
        if (!directory.cdUp()) {
            break;
        }
    }

    ManifestInfo result;
    result.warnings.append(
        QStringLiteral("日志附近未找到 build.ninja，步骤类型将按输出路径推断。"));
    return result;
}

void NinjaManifestParser::enrichRecords(QVector<NinjaLogRecord> &records,
                                        const ManifestInfo &manifest)
{
    for (NinjaLogRecord &record : records) {
        const QString rule = matchedRule(record, manifest);
        if (!rule.isEmpty()) {
            record.rule = rule;
            record.category = categoryFromRule(rule);
            record.classificationSource = ClassificationSource::ManifestRule;
        } else {
            record.rule.clear();
            record.category = categoryFromOutput(record.output);
            record.classificationSource = ClassificationSource::OutputHeuristic;
        }
    }
}

StepCategory NinjaManifestParser::categoryFromRule(const QString &rule)
{
    const QString upper = rule.toUpper();
    if (upper.contains(QStringLiteral("AUTOGEN"))
        || upper.contains(QStringLiteral("AUTOMOC"))
        || upper.contains(QStringLiteral("_MOC"))
        || upper.contains(QStringLiteral("_UIC"))) {
        return StepCategory::QtAutogen;
    }
    if (upper.contains(QStringLiteral("RCC"))
        || upper.contains(QStringLiteral("RESOURCE"))) {
        return StepCategory::Resource;
    }
    if (upper.contains(QStringLiteral("CUDA"))
        && upper.contains(QStringLiteral("COMPILER"))) {
        return StepCategory::CudaCompile;
    }
    if (upper.contains(QStringLiteral("CXX"))
        && upper.contains(QStringLiteral("COMPILER"))) {
        return StepCategory::CxxCompile;
    }
    if ((upper.startsWith(QStringLiteral("C_COMPILER"))
         || upper.contains(QStringLiteral("_C_COMPILER")))) {
        return StepCategory::CCompile;
    }
    if (upper.contains(QStringLiteral("STATIC_LIBRARY_LINKER"))
        || upper.contains(QStringLiteral("ARCHIVE"))) {
        return StepCategory::StaticLink;
    }
    if (upper.contains(QStringLiteral("SHARED_LIBRARY_LINKER"))
        || upper.contains(QStringLiteral("MODULE_LIBRARY_LINKER"))
        || upper.contains(QStringLiteral("SHARED_LINK"))) {
        return StepCategory::SharedLink;
    }
    if (upper.contains(QStringLiteral("EXECUTABLE_LINKER"))
        || upper.contains(QStringLiteral("LINK_EXECUTABLE"))) {
        return StepCategory::ExecutableLink;
    }
    if (upper.contains(QStringLiteral("CUSTOM_COMMAND"))
        || upper.contains(QStringLiteral("UTILITY"))) {
        return StepCategory::CustomCommand;
    }
    return StepCategory::Other;
}

StepCategory NinjaManifestParser::categoryFromOutput(const QString &output)
{
    const QString lower = QDir::fromNativeSeparators(output).toLower();
    const QString fileName = QFileInfo(lower).fileName();
    if (lower.contains(QStringLiteral("autogen"))
        || fileName.startsWith(QStringLiteral("moc_"))
        || fileName.startsWith(QStringLiteral("ui_"))
        || fileName.endsWith(QStringLiteral(".moc"))) {
        return StepCategory::QtAutogen;
    }
    if (fileName.endsWith(QStringLiteral(".rcc"))
        || fileName.contains(QStringLiteral("qrc_"))
        || fileName.endsWith(QStringLiteral("_resources.cpp"))) {
        return StepCategory::Resource;
    }
    if (lower.endsWith(QStringLiteral(".c.o"))
        || lower.endsWith(QStringLiteral(".c.obj"))) {
        return StepCategory::CCompile;
    }
    if (lower.endsWith(QStringLiteral(".o"))
        || lower.endsWith(QStringLiteral(".obj"))) {
        return StepCategory::CxxCompile;
    }
    if (lower.endsWith(QStringLiteral(".a"))
        || lower.endsWith(QStringLiteral(".lib"))) {
        return StepCategory::StaticLink;
    }
    if (lower.endsWith(QStringLiteral(".so"))
        || lower.contains(QStringLiteral(".so."))
        || lower.endsWith(QStringLiteral(".dylib"))
        || lower.endsWith(QStringLiteral(".dll"))
        || lower.endsWith(QStringLiteral(".bundle"))) {
        return StepCategory::SharedLink;
    }
    if (lower.endsWith(QStringLiteral(".exe"))) {
        return StepCategory::ExecutableLink;
    }
    return StepCategory::Other;
}

} // namespace ninja_analyzer
