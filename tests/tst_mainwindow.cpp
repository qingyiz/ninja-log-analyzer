#include "gui/MainWindow.h"
#include "gui/OverviewChartsWidget.h"
#include "gui/TimelineWidget.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QTableWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool writeText(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

} // namespace

class MainWindowTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsSwitchesBatchAndPreservesStateAfterFailure();
    void timelineLayoutUsesNonOverlappingLanesAndLimit();
    void usesProjectControlChrome();
    void renderConfiguredDemo();
};

void MainWindowTests::loadsSwitchesBatchAndPreservesStateAfterFailure()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDir root(temporary.path());
    const QString logPath = root.filePath(QStringLiteral(".ninja_log"));
    const QByteArray log =
        "# ninja log v5\n"
        "0\t100\t1\tobj/a.cpp.o\t1a\n"
        "10\t200\t1\tobj/b.cpp.o\t1b\n"
        "0\t50\t1\tobj/c.c.o\t1c\n"
        "20\t100\t1\tapp\t1d\n";
    const QByteArray manifest =
        "build obj/a.cpp.o: CXX_COMPILER__app a.cpp\n"
        "build obj/b.cpp.o: CXX_COMPILER__app b.cpp\n"
        "build obj/c.c.o: C_COMPILER__app c.c\n"
        "build app: CXX_EXECUTABLE_LINKER__app obj/a.cpp.o obj/c.c.o\n";
    QVERIFY(writeText(logPath, log));
    QVERIFY(writeText(root.filePath(QStringLiteral("build.ninja")), manifest));

    MainWindow window;
    QVERIFY(!window.hasLoadedAnalysis());
    QVERIFY2(window.analyzePath(temporary.path(), false), qPrintable(window.lastError()));
    QVERIFY(window.hasLoadedAnalysis());
    QCOMPARE(window.inferredBatchCount(), 2);
    QCOMPARE(window.currentTaskCount(), 2);
    QCOMPARE(window.filteredTaskCount(), 2);
    const QString successfulPath = window.currentLogPath();

    auto *summaryTask = window.findChild<QLabel *>(QStringLiteral("summaryTaskValue"));
    auto *conclusionTitle = window.findChild<QLabel *>(QStringLiteral("conclusionTitle"));
    auto *overviewCharts = window.findChild<OverviewChartsWidget *>(QStringLiteral("overviewCharts"));
    auto *categoryTable = window.findChild<QTableWidget *>(QStringLiteral("categoryTable"));
    auto *slowTasks = window.findChild<QTableView *>(QStringLiteral("slowTasksView"));
    auto *categoryFilter = window.findChild<QComboBox *>(QStringLiteral("categoryFilter"));
    auto *search = window.findChild<QLineEdit *>(QStringLiteral("outputSearch"));
    QVERIFY(summaryTask);
    QVERIFY(conclusionTitle);
    QVERIFY(overviewCharts);
    QVERIFY(categoryTable);
    QVERIFY(slowTasks);
    QVERIFY(categoryFilter);
    QVERIFY(search);
    auto *timeline = window.findChild<TimelineWidget *>(QStringLiteral("timelineWidget"));
    QVERIFY(timeline);
    QCOMPARE(summaryTask->text(), QStringLiteral("2"));
    QVERIFY(conclusionTitle->text().contains(QStringLiteral("2 个任务")));
    QCOMPARE(categoryTable->rowCount(), 2);
    QCOMPARE(overviewCharts->categoryCount(), 2);
    QCOMPARE(overviewCharts->taskBarCount(), 2);
    QCOMPARE(slowTasks->model()->rowCount(), 2);
    QCOMPARE(timeline->renderedRecordCount(), 2);

    auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("resultTabs"));
    QVERIFY(tabs);
    QVERIFY(tabs->tabText(1).contains(QStringLiteral("2")));
    QVERIFY(tabs->tabText(2).contains(QStringLiteral("2")));

    categoryTable->cellClicked(0, 0);
    QCoreApplication::processEvents();
    QCOMPARE(tabs->currentIndex(), 1);
    QCOMPARE(window.filteredTaskCount(), 1);
    categoryFilter->setCurrentIndex(0);
    tabs->setCurrentIndex(0);

    categoryFilter->setCurrentText(QStringLiteral("C 编译"));
    QCoreApplication::processEvents();
    QCOMPARE(window.filteredTaskCount(), 1);
    QCOMPARE(slowTasks->model()->rowCount(), 1);
    QCOMPARE(timeline->renderedRecordCount(), 1);
    QCOMPARE(summaryTask->text(), QStringLiteral("2"));

    categoryFilter->setCurrentIndex(0);
    search->setText(QStringLiteral("app"));
    QCoreApplication::processEvents();
    QCOMPARE(window.filteredTaskCount(), 1);
    QCOMPARE(summaryTask->text(), QStringLiteral("2"));
    search->clear();

    auto *batchCombo = window.findChild<QComboBox *>(QStringLiteral("batchCombo"));
    QVERIFY(batchCombo);
    batchCombo->setCurrentIndex(0);
    QCoreApplication::processEvents();
    QCOMPARE(window.currentTaskCount(), 4);
    QCOMPARE(summaryTask->text(), QStringLiteral("4"));
    QCOMPARE(overviewCharts->categoryCount(), 3);
    QCOMPARE(overviewCharts->taskBarCount(), 4);
    QCOMPARE(window.filteredTaskCount(), 4);
    QCOMPARE(timeline->renderedRecordCount(), 4);

    window.resize(820, 600);
    window.show();
    QTest::qWait(10);
    QVERIFY(window.centralWidget()->isVisible());

    QVERIFY(!window.analyzePath(root.filePath(QStringLiteral("missing")), false));
    QCOMPARE(window.currentLogPath(), successfulPath);
    QCOMPARE(window.currentTaskCount(), 4);
}

void MainWindowTests::timelineLayoutUsesNonOverlappingLanesAndLimit()
{
    QVector<ninja_analyzer::NinjaLogRecord> intervals(4);
    intervals[0].startMs = 0;
    intervals[0].endMs = 10;
    intervals[1].startMs = 5;
    intervals[1].endMs = 15;
    intervals[2].startMs = 10;
    intervals[2].endMs = 20;
    intervals[3].startMs = 20;
    intervals[3].endMs = 20;
    for (int index = 0; index < intervals.size(); ++index) {
        intervals[index].output = QStringLiteral("item-%1.o").arg(index);
    }

    const TimelineLayoutResult layout = TimelineWidget::buildLayout(intervals);
    QCOMPARE(layout.items.size(), 4);
    QCOMPARE(layout.laneCount, 2);
    QVERIFY(!layout.truncated);
    for (int leftIndex = 0; leftIndex < layout.items.size(); ++leftIndex) {
        for (int rightIndex = leftIndex + 1; rightIndex < layout.items.size(); ++rightIndex) {
            const TimelineLayoutItem &left = layout.items.at(leftIndex);
            const TimelineLayoutItem &right = layout.items.at(rightIndex);
            if (left.lane != right.lane || left.record.durationMs() <= 0
                || right.record.durationMs() <= 0) {
                continue;
            }
            QVERIFY(left.record.endMs <= right.record.startMs
                    || right.record.endMs <= left.record.startMs);
        }
    }

    QVector<ninja_analyzer::NinjaLogRecord> many;
    many.reserve(TimelineWidget::MaximumRenderedRecords + 2);
    for (int index = 0; index < TimelineWidget::MaximumRenderedRecords + 2; ++index) {
        ninja_analyzer::NinjaLogRecord record;
        record.startMs = index;
        record.endMs = index + index % 17;
        record.output = QString::number(index);
        many.append(record);
    }
    const TimelineLayoutResult limited = TimelineWidget::buildLayout(many);
    QCOMPARE(limited.items.size(), TimelineWidget::MaximumRenderedRecords);
    QCOMPARE(limited.totalInputCount, TimelineWidget::MaximumRenderedRecords + 2);
    QVERIFY(limited.truncated);
}

void MainWindowTests::usesProjectControlChrome()
{
    MainWindow window;
    auto *batchCombo = window.findChild<QComboBox *>(QStringLiteral("batchCombo"));
    auto *categoryFilter = window.findChild<QComboBox *>(QStringLiteral("categoryFilter"));
    auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("resultTabs"));
    QVERIFY(batchCombo);
    QVERIFY(categoryFilter);
    QVERIFY(tabs);
    QCOMPARE(tabs->tabBar()->objectName(), QStringLiteral("resultTabBar"));
    QVERIFY(QFile::exists(QStringLiteral(":/ninja-analyzer/ui/chevron-down.svg")));

    const QString style = window.styleSheet();
    QVERIFY(style.contains(QStringLiteral("QComboBox::drop-down")));
    QVERIFY(style.contains(QStringLiteral("QComboBox::down-arrow")));
    QVERIFY(style.contains(QStringLiteral("QTabBar#resultTabBar::tab:selected")));

    tabs->setCurrentIndex(2);
    QCOMPARE(tabs->currentIndex(), 2);
}

void MainWindowTests::renderConfiguredDemo()
{
    const QString demoPath = qEnvironmentVariable("NINJA_ANALYZER_DEMO_PATH");
    const QString capturePath = qEnvironmentVariable("NINJA_ANALYZER_CAPTURE_PATH");
    if (demoPath.isEmpty() || capturePath.isEmpty()) {
        QSKIP("仅在设置 demo/capture 环境变量时执行视觉快照验收", 0);
    }

    MainWindow window;
    window.resize(1600, 1000);
    QVERIFY2(window.analyzePath(demoPath, false), qPrintable(window.lastError()));
    window.show();
    QTest::qWait(80);
    QVERIFY2(window.grab().save(capturePath), qPrintable(capturePath));

    auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("resultTabs"));
    QVERIFY(tabs);
    tabs->setCurrentIndex(2);
    QTest::qWait(80);
    const QFileInfo captureInfo(capturePath);
    const QString timelinePath = captureInfo.absolutePath()
                                 + QLatin1Char('/') + captureInfo.completeBaseName()
                                 + QStringLiteral("-timeline.png");
    QVERIFY2(window.grab().save(timelinePath), qPrintable(timelinePath));
}

QTEST_MAIN(MainWindowTests)
#include "tst_mainwindow.moc"
