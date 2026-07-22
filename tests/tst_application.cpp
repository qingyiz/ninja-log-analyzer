#include "application/AnalysisService.h"

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
    QCOMPARE(result.value.logVersion, 5);
    QCOMPARE(result.value.records.size(), 19);
    QCOMPARE(result.value.batches.size(), 2);
    QCOMPARE(result.value.batches.last().recordCount, 14);
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    AnalysisServiceTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_application.moc"
