#include "gui/AnalysisResultsWidget.h"

#include "gui/OverviewPage.h"
#include "gui/SlowTasksPage.h"
#include "gui/TimelinePage.h"

#include <QTabBar>

AnalysisResultsWidget::AnalysisResultsWidget(QWidget *parent)
    : QTabWidget(parent)
{
    setObjectName(QStringLiteral("resultTabs"));
    tabBar()->setObjectName(QStringLiteral("resultTabBar"));
    tabBar()->setDrawBase(false);
    tabBar()->setExpanding(false);
    tabBar()->setUsesScrollButtons(false);
    overview_ = new OverviewPage(this);
    slowTasks_ = new SlowTasksPage(this);
    timeline_ = new TimelinePage(this);
    addTab(overview_, tr("概览"));
    addTab(slowTasks_, tr("最慢步骤"));
    addTab(timeline_, tr("并发时间线"));
    connect(overview_, &OverviewPage::categoryActivated,
            this, &AnalysisResultsWidget::categoryActivated);
    connect(overview_, &OverviewPage::taskActivated,
            this, &AnalysisResultsWidget::taskActivated);
    connect(overview_, &OverviewPage::showSlowTasksRequested, this, [this] {
        setCurrentIndex(1);
    });
    connect(overview_, &OverviewPage::showTimelineRequested, this, [this] {
        setCurrentIndex(2);
    });
}

void AnalysisResultsWidget::setAnalysis(const ninja_analyzer::AnalysisResult &analysis)
{
    overview_->setAnalysis(analysis);
}

void AnalysisResultsWidget::setFilteredRecords(
    const QVector<ninja_analyzer::NinjaLogRecord> &records,
    int totalTaskCount)
{
    slowTasks_->setRecords(records, totalTaskCount);
    timeline_->setRecords(records);
    setTabText(1, tr("慢任务  %1").arg(records.size()));
    setTabText(2, tr("并发时间线  %1").arg(records.size()));
}
