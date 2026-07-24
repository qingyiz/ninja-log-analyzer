#include "application/AnalysisReportExporter.h"
#include "application/AnalysisService.h"
#include "application/MachineLoadProbe.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace ninja_analyzer;

namespace {

bool writeText(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

} // namespace

class AnalysisServiceTests final : public QObject {
    Q_OBJECT

private slots:
    void locatesAndLoadsCompleteAnalysis();
    void rejectsInvalidLogWithoutPartialValue();
    void loadsRepositoryDemo();
    void capturesMachineLoadSnapshot();
    void exportsCompleteEscapedHtmlReport();
    void rejectsInvalidReportRequests();
};

void AnalysisServiceTests::locatesAndLoadsCompleteAnalysis()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDir root(temporary.path());
    const QString logPath = root.filePath(QStringLiteral(".ninja_log"));
    QVERIFY(writeText(logPath,
                      "# ninja log v5\n"
                      "0\t100\t1\tobj/a.cpp.o\t1a\n"
                      "10\t200\t1\tobj/b.cpp.o\t1b\n"
                      "broken\n"
                      "0\t50\t1\tapp\t1c\n"));
    QVERIFY(writeText(root.filePath(QStringLiteral("build.ninja")),
                      "build obj/a.cpp.o: CXX_COMPILER a.cpp\n"
                      "build obj/b.cpp.o: CXX_COMPILER b.cpp\n"
                      "build app: CXX_EXECUTABLE_LINKER obj/a.cpp.o\n"));

    const AnalysisService service;
    const LocateResult located = service.locateLogs(temporary.path());
    QVERIFY2(located.ok(), qPrintable(located.error));
    QCOMPARE(located.logPaths, QStringList{QFileInfo(logPath).canonicalFilePath()});

    const AnalysisLoadResult result = service.loadLog(located.logPaths.first());
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.value.logPath, QFileInfo(logPath).canonicalFilePath());
    QCOMPARE(result.value.logVersion, 5);
    QCOMPARE(result.value.records.size(), 3);
    QCOMPARE(result.value.parseWarnings.size(), 1);
    QCOMPARE(result.value.batches.size(), 2);
    QVERIFY(result.value.manifest.found());
    QCOMPARE(result.value.records.first().rule, QStringLiteral("CXX_COMPILER"));
    QCOMPARE(result.value.records.first().category, StepCategory::CxxCompile);
    QVERIFY(result.value.machineLoad.capturedAtUtc.isValid());
    QVERIFY(!result.value.machineLoad.platformDescription().isEmpty());
    QVERIFY(!result.value.machineLoad.loadAverageDescription().isEmpty());
}

void AnalysisServiceTests::rejectsInvalidLogWithoutPartialValue()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString logPath = QDir(temporary.path()).filePath(QStringLiteral(".ninja_log"));
    QVERIFY(writeText(logPath, "# ninja log v3\n0\t1\t1\toutput\t1a\n"));

    const AnalysisLoadResult result = AnalysisService().loadLog(logPath);
    QVERIFY(!result.ok());
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.value.logPath.isEmpty());
    QVERIFY(result.value.records.isEmpty());
    QVERIFY(result.value.batches.isEmpty());
}

void AnalysisServiceTests::loadsRepositoryDemo()
{
    const QString demoPath = QFINDTESTDATA("../examples/demo-build/.ninja_log");
    QVERIFY2(!demoPath.isEmpty(), "找不到仓库 demo .ninja_log");

    const AnalysisLoadResult result = AnalysisService().loadLog(demoPath);
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.value.logVersion, 7);
    QCOMPARE(result.value.records.size(), 19);
    QCOMPARE(result.value.batches.size(), 2);
    QCOMPARE(result.value.batches.last().recordCount, 14);
}

void AnalysisServiceTests::capturesMachineLoadSnapshot()
{
    const MachineLoadSnapshot snapshot = MachineLoadProbe::capture();
    QVERIFY(snapshot.capturedAtUtc.isValid());
    QCOMPARE(snapshot.capturedAtUtc.timeSpec(), Qt::UTC);
    QVERIFY(snapshot.logicalProcessorCount != 0);
    QVERIFY(!snapshot.platformDescription().isEmpty());
    QVERIFY(!snapshot.loadAverageDescription().isEmpty());
    QVERIFY(MachineLoadSnapshot::limitationText().contains(QStringLiteral("不是构建时")));
    if (!snapshot.loadAverageAvailable) {
        QVERIFY(snapshot.loadAverageDescription().contains(QStringLiteral("不可用")));
    }
}

void AnalysisServiceTests::exportsCompleteEscapedHtmlReport()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDir root(temporary.path());
    const QString logPath = root.filePath(QStringLiteral("renamed-build-log.txt"));
    QVERIFY(writeText(logPath,
                      "# ninja log v7\n"
                      "0\t100\t1\tobj/<one>&.o\t1a\n"
                      "10\t250\t1\tbin/two\t1b\n"));

    const AnalysisService service;
    const AnalysisLoadResult loaded = service.loadLog(logPath);
    QVERIFY2(loaded.ok(), qPrintable(loaded.error));
    const QVector<NinjaLogRecord> records = service.recordsForBatch(loaded.value, -1);

    AnalysisReportRequest request;
    request.outputPath = root.filePath(QStringLiteral("complete-report.html"));
    request.loaded = loaded.value;
    request.batchIndex = -1;
    request.analysis = service.analyze(records);
    const AnalysisReportResult exported = AnalysisReportExporter::exportHtml(request);
    QVERIFY2(exported.ok(), qPrintable(exported.error));

    QFile report(exported.outputPath);
    QVERIFY(report.open(QIODevice::ReadOnly));
    const QString html = QString::fromUtf8(report.readAll());
    QVERIFY(html.contains(QStringLiteral("Ninja 构建完整分析报告")));
    QVERIFY(html.contains(QStringLiteral("Ninja log v7")));
    QVERIFY(html.contains(QStringLiteral("分析机器负载快照")));
    QVERIFY(html.contains(QStringLiteral("泳道是为了")));
    QCOMPARE(html.count(QStringLiteral("data-task-row=\"1\"")), 2);
    QVERIFY(html.contains(QStringLiteral("obj/&lt;one&gt;&amp;.o")));
    QVERIFY(!html.contains(QStringLiteral("obj/<one>&.o")));
}

void AnalysisServiceTests::rejectsInvalidReportRequests()
{
    AnalysisReportRequest empty;
    QVERIFY(!AnalysisReportExporter::exportHtml(empty).ok());

    empty.outputPath = QStringLiteral("/this/path/does/not/exist/report.html");
    QVERIFY(!AnalysisReportExporter::exportHtml(empty).ok());
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    AnalysisServiceTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_application.moc"
