#include "core/BuildAnalyzer.h"
#include "core/LogLocator.h"
#include "core/NinjaLogParser.h"
#include "core/NinjaLogTypes.h"
#include "core/NinjaManifestParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QtTest>

using namespace ninja_analyzer;

namespace {

bool writeBytes(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

class CoreTests final : public QObject {
    Q_OBJECT

private slots:
    void typeSmokeTest();
    void locatorAcceptsFileAndSortsNestedLogs();
    void locatorReportsInvalidInputs();
    void parserReadsV5CrLfAndPreservesInput();
    void parserReadsV7CommandHash();
    void parserReadsV4CommandWithTabs();
    void parserIsolatesMalformedLines();
    void parserReportsFatalFormatErrors();
    void manifestParsesOutputsIncludesAndEscapes();
    void manifestSearchesParentsAndFallsBack();
    void classifierCoversRuleAndOutputFamilies();
    void analyzerPartitionsBatchesWithoutLoss();
    void analyzerComputesConservationAndConcurrency();
    void analyzerHandlesAdjacentAndZeroIntervals();
    void analyzerSortsSlowTasksAndBuildsInsights();
    void analyzerFormatsDurations();
    void analyzerHandlesHundredThousandRecords();
    void parserAndAnalyzerHandleHundredThousandRecords();
};

void CoreTests::typeSmokeTest()
{
    NinjaLogRecord record;
    record.startMs = 125;
    record.endMs = 725;
    record.category = StepCategory::CxxCompile;

    QCOMPARE(record.durationMs(), qint64(600));
    QCOMPARE(categoryDisplayName(record.category), QStringLiteral("C++ 编译"));
}

void CoreTests::locatorAcceptsFileAndSortsNestedLogs()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QDir root(temporary.path());
    QVERIFY(root.mkpath(QStringLiteral("z-build")));
    QVERIFY(root.mkpath(QStringLiteral("a-build/deep")));

    const QString first = root.filePath(QStringLiteral("a-build/deep/classified-build.log"));
    const QString second = root.filePath(QStringLiteral("z-build/saved-without-extension"));
    QVERIFY(writeBytes(first, "# ninja log v5\n"));
    QVERIFY(writeBytes(second, "# ninja log v7\n"));
    QVERIFY(writeBytes(root.filePath(QStringLiteral("ignore.txt")), "not ninja\n"));

    const LocateResult directoryResult = LogLocator::resolve(temporary.path());
    QVERIFY2(directoryResult.ok(), qPrintable(directoryResult.error));
    QCOMPARE(directoryResult.logPaths.size(), 2);
    QCOMPARE(directoryResult.logPaths.at(0), QFileInfo(first).canonicalFilePath());
    QCOMPARE(directoryResult.logPaths.at(1), QFileInfo(second).canonicalFilePath());

    const LocateResult fileResult = LogLocator::resolve(first);
    QVERIFY(fileResult.ok());
    QCOMPARE(fileResult.logPaths, QStringList{QFileInfo(first).canonicalFilePath()});
}

void CoreTests::locatorReportsInvalidInputs()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString wrongFile = QDir(temporary.path()).filePath(QStringLiteral("build.log"));
    QVERIFY(writeBytes(wrongFile, "not ninja"));

    QVERIFY(!LogLocator::resolve(QString()).ok());
    QVERIFY(!LogLocator::resolve(QDir(temporary.path()).filePath(QStringLiteral("missing"))).ok());
    QVERIFY(!LogLocator::resolve(wrongFile).ok());
    QVERIFY(!LogLocator::resolve(temporary.path()).ok());
}

void CoreTests::parserReadsV5CrLfAndPreservesInput()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString logPath = QDir(temporary.path()).filePath(QStringLiteral(".ninja_log"));
    const QByteArray contents =
        "# ninja log v5\r\n"
        "10\t210\t1700000000\tobj/a file.cpp.o\tAbC123\r\n"
        "210\t1210\t1700001000\tbin/demo app\t0\r\n";
    QVERIFY(writeBytes(logPath, contents));

    const ParseResult result = NinjaLogParser::parse(logPath);
    QVERIFY2(result.ok(), qPrintable(result.fatalError));
    QCOMPARE(result.version, 5);
    QCOMPARE(result.records.size(), 2);
    QCOMPARE(result.warnings.size(), 0);
    QCOMPARE(result.records.at(0).sourceLine, 2);
    QCOMPARE(result.records.at(0).output, QStringLiteral("obj/a file.cpp.o"));
    QCOMPARE(result.records.at(0).commandHash, quint64(0xabc123));
    QVERIFY(result.records.at(0).hasCommandHash);
    QCOMPARE(result.records.at(1).durationMs(), qint64(1000));
    QCOMPARE(readBytes(logPath), contents);
}

void CoreTests::parserReadsV7CommandHash()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString logPath =
        QDir(temporary.path()).filePath(QStringLiteral("2026-07-24-full-rebuild.log"));
    const QByteArray contents =
        "# ninja log v7\n"
        "5\t1005\t1700000000\tobj/v7.cpp.o\tF00dBeef\n"
        "0\t10\t1700000001\tobj/bad-v7.o\tnot-hex\n";
    QVERIFY(writeBytes(logPath, contents));

    const ParseResult result = NinjaLogParser::parse(logPath);
    QVERIFY2(result.ok(), qPrintable(result.fatalError));
    QCOMPARE(result.version, 7);
    QCOMPARE(result.records.size(), 1);
    QCOMPARE(result.warnings.size(), 1);
    QVERIFY(result.records.first().hasCommandHash);
    QCOMPARE(result.records.first().commandHash, quint64(0xf00dbeef));
    QCOMPARE(result.records.first().durationMs(), qint64(1000));
}

void CoreTests::parserReadsV4CommandWithTabs()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString logPath = QDir(temporary.path()).filePath(QStringLiteral(".ninja_log"));
    const QByteArray contents =
        "# ninja log v4\n"
        "0\t50\t123\tobj/a.o\tclang\t-DVALUE=1 -c a.c\n";
    QVERIFY(writeBytes(logPath, contents));

    const ParseResult result = NinjaLogParser::parse(logPath);
    QVERIFY2(result.ok(), qPrintable(result.fatalError));
    QCOMPARE(result.version, 4);
    QCOMPARE(result.records.size(), 1);
    QCOMPARE(result.records.first().commandField,
             QStringLiteral("clang\t-DVALUE=1 -c a.c"));
    QVERIFY(!result.records.first().hasCommandHash);
}

void CoreTests::parserIsolatesMalformedLines()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString logPath = QDir(temporary.path()).filePath(QStringLiteral(".ninja_log"));
    const QByteArray contents =
        "# ninja log v5\n"
        "missing-fields\n"
        "x\t20\t1\tobj/a.o\t1a\n"
        "30\t20\t1\tobj/b.o\t1b\n"
        "0\t20\t1\t\t1c\n"
        "0\t20\t1\tobj/c.o\tnot-hex\n"
        "0\t20\t1\tobj/good.o\tff\n"
        "truncated\t0";
    QVERIFY(writeBytes(logPath, contents));

    const ParseResult result = NinjaLogParser::parse(logPath);
    QVERIFY2(result.ok(), qPrintable(result.fatalError));
    QCOMPARE(result.records.size(), 1);
    QCOMPARE(result.records.first().output, QStringLiteral("obj/good.o"));
    QCOMPARE(result.warnings.size(), 6);
}

void CoreTests::parserReportsFatalFormatErrors()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDir root(temporary.path());

    const QString emptyPath = root.filePath(QStringLiteral("empty/.ninja_log"));
    QVERIFY(QDir().mkpath(QFileInfo(emptyPath).absolutePath()));
    QVERIFY(writeBytes(emptyPath, QByteArray()));
    QVERIFY(!NinjaLogParser::parse(emptyPath).ok());

    const QString missingSignature = root.filePath(QStringLiteral("missing-signature/.ninja_log"));
    QVERIFY(QDir().mkpath(QFileInfo(missingSignature).absolutePath()));
    QVERIFY(writeBytes(missingSignature, "0\t1\t2\ta.o\tff\n"));
    QVERIFY(!NinjaLogParser::parse(missingSignature).ok());

    const QString unsupported = root.filePath(QStringLiteral("unsupported/.ninja_log"));
    QVERIFY(QDir().mkpath(QFileInfo(unsupported).absolutePath()));
    QVERIFY(writeBytes(unsupported, "# ninja log v6\n0\t1\t2\ta.o\tff\n"));
    const ParseResult unsupportedResult = NinjaLogParser::parse(unsupported);
    QVERIFY(!unsupportedResult.ok());
    QVERIFY(unsupportedResult.fatalError.contains(QStringLiteral("v4、v5 和 v7")));

    const QString noValidRows = root.filePath(QStringLiteral("bad-rows/.ninja_log"));
    QVERIFY(QDir().mkpath(QFileInfo(noValidRows).absolutePath()));
    QVERIFY(writeBytes(noValidRows, "# ninja log v5\nbad\n"));
    const ParseResult badRows = NinjaLogParser::parse(noValidRows);
    QVERIFY(!badRows.ok());
    QCOMPARE(badRows.warnings.size(), 1);

    QVERIFY(!NinjaLogParser::parse(root.filePath(QStringLiteral("not-there"))).ok());
}

void CoreTests::manifestParsesOutputsIncludesAndEscapes()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QDir root(temporary.path());
    QVERIFY(root.mkpath(QStringLiteral("sub")));

    const QString manifestPath = root.filePath(QStringLiteral("build.ninja"));
    const QString includePath = root.filePath(QStringLiteral("sub/extra.ninja"));
    const QString logPath = root.filePath(QStringLiteral(".ninja_log"));
    const QByteArray manifest =
        "include sub/extra.ninja\n"
        "build obj/a$ file.cpp.o | obj/a.side: $\n"
        "  CXX_COMPILER__demo src/a$ file.cpp\n"
        "build libdemo.a: CXX_STATIC_LIBRARY_LINKER__demo obj/a$ file.cpp.o\n"
        "build demo: CXX_EXECUTABLE_LINKER__demo libdemo.a\n";
    const QByteArray included =
        "build generated/resources.rcc: CUSTOM_COMMAND assets.qrc\n";
    QVERIFY(writeBytes(manifestPath, manifest));
    QVERIFY(writeBytes(includePath, included));
    QVERIFY(writeBytes(logPath, "# ninja log v5\n"));

    const ManifestInfo info = NinjaManifestParser::loadNear(logPath);
    QVERIFY(info.found());
    QCOMPARE(info.ruleByOutput.value(QStringLiteral("obj/a file.cpp.o")),
             QStringLiteral("CXX_COMPILER__demo"));
    QCOMPARE(info.ruleByOutput.value(QStringLiteral("obj/a.side")),
             QStringLiteral("CXX_COMPILER__demo"));
    QCOMPARE(info.ruleByOutput.value(QStringLiteral("generated/resources.rcc")),
             QStringLiteral("CUSTOM_COMMAND"));

    QVector<NinjaLogRecord> records(3);
    records[0].output = QStringLiteral("obj/a file.cpp.o");
    records[1].output = QStringLiteral("libdemo.a");
    records[2].output = QStringLiteral("missing.c.o");
    NinjaManifestParser::enrichRecords(records, info);
    QCOMPARE(records.at(0).category, StepCategory::CxxCompile);
    QCOMPARE(records.at(0).classificationSource, ClassificationSource::ManifestRule);
    QCOMPARE(records.at(1).category, StepCategory::StaticLink);
    QCOMPARE(records.at(2).category, StepCategory::CCompile);
    QCOMPARE(records.at(2).classificationSource, ClassificationSource::OutputHeuristic);
}

void CoreTests::manifestSearchesParentsAndFallsBack()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QDir root(temporary.path());
    QVERIFY(root.mkpath(QStringLiteral("logs/private")));
    const QString manifestPath = root.filePath(QStringLiteral("build.ninja"));
    const QString logPath = root.filePath(QStringLiteral("logs/private/.ninja_log"));
    QVERIFY(writeBytes(manifestPath, "build app.exe: CXX_EXECUTABLE_LINKER__app x.o\n"));
    QVERIFY(writeBytes(logPath, "# ninja log v5\n"));

    const ManifestInfo parentInfo = NinjaManifestParser::loadNear(logPath);
    QVERIFY(parentInfo.found());
    QCOMPARE(parentInfo.ruleByOutput.value(QStringLiteral("app.exe")),
             QStringLiteral("CXX_EXECUTABLE_LINKER__app"));

    const QString isolatedDir = root.filePath(QStringLiteral("outside/deeper"));
    QVERIFY(root.mkpath(QStringLiteral("outside/deeper")));
    const QString isolatedLog = QDir(isolatedDir).filePath(QStringLiteral(".ninja_log"));
    QVERIFY(writeBytes(isolatedLog, "# ninja log v5\n"));
    QFile::remove(manifestPath);
    const ManifestInfo missingInfo = NinjaManifestParser::loadNear(isolatedLog);
    QVERIFY(!missingInfo.found());
    QVERIFY(!missingInfo.warnings.isEmpty());

    QVector<NinjaLogRecord> records(1);
    records[0].output = QStringLiteral("bin/plugin.dylib");
    NinjaManifestParser::enrichRecords(records, missingInfo);
    QCOMPARE(records.first().category, StepCategory::SharedLink);
    QCOMPARE(records.first().classificationSource,
             ClassificationSource::OutputHeuristic);
}

void CoreTests::classifierCoversRuleAndOutputFamilies()
{
    QCOMPARE(NinjaManifestParser::categoryFromRule(QStringLiteral("C_COMPILER__x")),
             StepCategory::CCompile);
    QCOMPARE(NinjaManifestParser::categoryFromRule(QStringLiteral("CXX_COMPILER__x")),
             StepCategory::CxxCompile);
    QCOMPARE(NinjaManifestParser::categoryFromRule(QStringLiteral("CUDA_COMPILER__x")),
             StepCategory::CudaCompile);
    QCOMPARE(NinjaManifestParser::categoryFromRule(QStringLiteral("x_autogen")),
             StepCategory::QtAutogen);
    QCOMPARE(NinjaManifestParser::categoryFromRule(QStringLiteral("RCC_x")),
             StepCategory::Resource);
    QCOMPARE(NinjaManifestParser::categoryFromRule(
                 QStringLiteral("CXX_SHARED_LIBRARY_LINKER__x")),
             StepCategory::SharedLink);
    QCOMPARE(NinjaManifestParser::categoryFromRule(
                 QStringLiteral("CXX_EXECUTABLE_LINKER__x")),
             StepCategory::ExecutableLink);
    QCOMPARE(NinjaManifestParser::categoryFromRule(QStringLiteral("CUSTOM_COMMAND")),
             StepCategory::CustomCommand);
    QCOMPARE(NinjaManifestParser::categoryFromRule(QStringLiteral("unknown_rule")),
             StepCategory::Other);

    QCOMPARE(NinjaManifestParser::categoryFromOutput(QStringLiteral("a.cpp.o")),
             StepCategory::CxxCompile);
    QCOMPARE(NinjaManifestParser::categoryFromOutput(QStringLiteral("a.c.obj")),
             StepCategory::CCompile);
    QCOMPARE(NinjaManifestParser::categoryFromOutput(QStringLiteral("moc_widget.cpp")),
             StepCategory::QtAutogen);
    QCOMPARE(NinjaManifestParser::categoryFromOutput(QStringLiteral("qrc_icons.cpp")),
             StepCategory::Resource);
    QCOMPARE(NinjaManifestParser::categoryFromOutput(QStringLiteral("demo.exe")),
             StepCategory::ExecutableLink);
    QCOMPARE(NinjaManifestParser::categoryFromOutput(QStringLiteral("no-extension")),
             StepCategory::Other);
}

void CoreTests::analyzerPartitionsBatchesWithoutLoss()
{
    QVector<NinjaLogRecord> records(5);
    const qint64 ends[] = {10, 20, 5, 6, 3};
    for (int index = 0; index < records.size(); ++index) {
        records[index].startMs = 0;
        records[index].endMs = ends[index];
        records[index].output = QString::number(index);
    }

    const QVector<BuildBatch> batches = BuildAnalyzer::partitionBatches(records);
    QCOMPARE(batches.size(), 3);
    QCOMPARE(batches.at(0).firstRecord, 0);
    QCOMPARE(batches.at(0).recordCount, 2);
    QCOMPARE(batches.at(1).firstRecord, 2);
    QCOMPARE(batches.at(1).recordCount, 2);
    QCOMPARE(batches.at(2).firstRecord, 4);
    QCOMPARE(batches.at(2).recordCount, 1);

    QVector<NinjaLogRecord> reconstructed;
    for (const BuildBatch &batch : batches) {
        const QVector<NinjaLogRecord> part = BuildAnalyzer::recordsForBatch(records, batch);
        for (const NinjaLogRecord &record : part) {
            reconstructed.append(record);
        }
    }
    QCOMPARE(reconstructed.size(), records.size());
    for (int index = 0; index < records.size(); ++index) {
        QCOMPARE(reconstructed.at(index).output, records.at(index).output);
    }
    QVERIFY(BuildAnalyzer::partitionBatches({}).isEmpty());
}

void CoreTests::analyzerComputesConservationAndConcurrency()
{
    QVector<NinjaLogRecord> records(4);
    records[0].startMs = 0;
    records[0].endMs = 10;
    records[0].output = QStringLiteral("a.o");
    records[0].category = StepCategory::CxxCompile;
    records[1].startMs = 5;
    records[1].endMs = 15;
    records[1].output = QStringLiteral("b.o");
    records[1].category = StepCategory::CxxCompile;
    records[2].startMs = 10;
    records[2].endMs = 20;
    records[2].output = QStringLiteral("c.o");
    records[2].category = StepCategory::CCompile;
    records[3].startMs = 10;
    records[3].endMs = 10;
    records[3].output = QStringLiteral("zero.o");
    records[3].category = StepCategory::CCompile;

    const AnalysisResult result = BuildAnalyzer::analyze(records);
    QCOMPARE(result.summary.taskCount, 4);
    QCOMPARE(result.summary.observedSpanMs, qint64(20));
    QCOMPARE(result.summary.totalTaskMs, qint64(30));
    QCOMPARE(result.summary.averageParallelism, 1.5);
    QCOMPARE(result.summary.maximumParallelism, 2);

    int countSum = 0;
    qint64 durationSum = 0;
    for (const CategoryStats &stats : result.categories) {
        countSum += stats.count;
        durationSum += stats.totalMs;
    }
    QCOMPARE(countSum, result.summary.taskCount);
    QCOMPARE(durationSum, result.summary.totalTaskMs);
    QCOMPARE(result.categories.size(), 2);
}

void CoreTests::analyzerHandlesAdjacentAndZeroIntervals()
{
    QVector<NinjaLogRecord> records(3);
    records[0].startMs = 0;
    records[0].endMs = 10;
    records[1].startMs = 10;
    records[1].endMs = 20;
    records[2].startMs = 20;
    records[2].endMs = 20;
    for (int index = 0; index < records.size(); ++index) {
        records[index].output = QString::number(index);
    }

    const AnalysisResult result = BuildAnalyzer::analyze(records);
    QCOMPARE(result.summary.maximumParallelism, 1);
    QCOMPARE(result.summary.averageParallelism, 1.0);

    QVector<NinjaLogRecord> allZero(2);
    allZero[0].startMs = allZero[0].endMs = 5;
    allZero[1].startMs = allZero[1].endMs = 5;
    const AnalysisResult zeroResult = BuildAnalyzer::analyze(allZero);
    QCOMPARE(zeroResult.summary.observedSpanMs, qint64(0));
    QCOMPARE(zeroResult.summary.averageParallelism, 0.0);
    QCOMPARE(zeroResult.summary.maximumParallelism, 0);
}

void CoreTests::analyzerSortsSlowTasksAndBuildsInsights()
{
    QVector<NinjaLogRecord> records(5);
    for (int index = 0; index < records.size(); ++index) {
        records[index].startMs = index * 1000;
        records[index].endMs = records[index].startMs + 100;
        records[index].output = QStringLiteral("short-%1.o").arg(index);
        records[index].sourceLine = index + 2;
        records[index].category = StepCategory::CxxCompile;
    }
    records[3].startMs = 5000;
    records[3].endMs = 10000;
    records[3].output = QStringLiteral("z-long.o");
    records[4].startMs = 5000;
    records[4].endMs = 10000;
    records[4].output = QStringLiteral("a-long.o");
    records[4].category = StepCategory::ExecutableLink;

    const AnalysisResult result = BuildAnalyzer::analyze(records);
    QCOMPARE(result.slowest.at(0).output, QStringLiteral("a-long.o"));
    QCOMPARE(result.slowest.at(1).output, QStringLiteral("z-long.o"));
    QVERIFY(result.insights.size() >= 3);

    QStringList titles;
    for (const Insight &insight : result.insights) {
        titles.append(insight.title);
    }
    QVERIFY(titles.contains(QStringLiteral("累计耗时最高类型")));
    QVERIFY(titles.contains(QStringLiteral("最慢单步")));
    QVERIFY(titles.contains(QStringLiteral("尾段长任务候选")));
}

void CoreTests::analyzerFormatsDurations()
{
    QCOMPARE(BuildAnalyzer::formatDuration(999), QStringLiteral("999 ms"));
    QCOMPARE(BuildAnalyzer::formatDuration(1250), QStringLiteral("1.25 s"));
    QCOMPARE(BuildAnalyzer::formatDuration(12000), QStringLiteral("12 s"));
    QCOMPARE(BuildAnalyzer::formatDuration(60000), QStringLiteral("1 min"));
    QCOMPARE(BuildAnalyzer::formatDuration(62500), QStringLiteral("1 min 2.5 s"));
    QCOMPARE(BuildAnalyzer::formatDuration(-1), QStringLiteral("0 ms"));
}

void CoreTests::analyzerHandlesHundredThousandRecords()
{
    QVector<NinjaLogRecord> records;
    records.reserve(100000);
    for (int index = 0; index < 100000; ++index) {
        NinjaLogRecord record;
        record.startMs = index % 10000;
        record.endMs = record.startMs + (index % 101);
        record.output = QStringLiteral("obj/%1.cpp.o").arg(index);
        record.sourceLine = index + 2;
        record.category = index % 7 == 0 ? StepCategory::CCompile
                                         : StepCategory::CxxCompile;
        records.append(record);
    }

    QElapsedTimer timer;
    timer.start();
    const AnalysisResult result = BuildAnalyzer::analyze(records);
    const qint64 elapsedMs = timer.elapsed();
    QCOMPARE(result.summary.taskCount, 100000);
    QVERIFY2(elapsedMs < 2000,
             qPrintable(QStringLiteral("10 万条统计耗时 %1 ms").arg(elapsedMs)));
}

void CoreTests::parserAndAnalyzerHandleHundredThousandRecords()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString logPath = QDir(temporary.path()).filePath(QStringLiteral(".ninja_log"));
    QByteArray contents("# ninja log v5\n");
    contents.reserve(7 * 1024 * 1024);
    for (int index = 0; index < 100000; ++index) {
        const qint64 start = index % 10000;
        const qint64 end = start + index % 101;
        contents += QByteArray::number(start) + '\t'
                    + QByteArray::number(end) + "\t1\tobj/"
                    + QByteArray::number(index) + ".cpp.o\t"
                    + QByteArray::number(static_cast<qulonglong>(index + 1), 16) + '\n';
    }
    QVERIFY(writeBytes(logPath, contents));

    QElapsedTimer timer;
    timer.start();
    ParseResult parsed = NinjaLogParser::parse(logPath);
    QVERIFY2(parsed.ok(), qPrintable(parsed.fatalError));
    ManifestInfo noManifest;
    NinjaManifestParser::enrichRecords(parsed.records, noManifest);
    const AnalysisResult result = BuildAnalyzer::analyze(parsed.records);
    const qint64 elapsedMs = timer.elapsed();

    QCOMPARE(parsed.records.size(), 100000);
    QCOMPARE(result.summary.taskCount, 100000);
    QVERIFY2(elapsedMs < 2000,
             qPrintable(QStringLiteral("10 万条解析+分类+统计耗时 %1 ms").arg(elapsedMs)));
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    CoreTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_core.moc"
