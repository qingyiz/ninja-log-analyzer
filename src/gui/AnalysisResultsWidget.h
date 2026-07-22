#pragma once

#include "core/NinjaLogTypes.h"

#include <QTabWidget>

class OverviewPage;
class SlowTasksPage;
class TimelinePage;

class AnalysisResultsWidget final : public QTabWidget {
    Q_OBJECT

public:
    explicit AnalysisResultsWidget(QWidget *parent = nullptr);
    void setAnalysis(const ninja_analyzer::AnalysisResult &analysis);
    void setFilteredRecords(const QVector<ninja_analyzer::NinjaLogRecord> &records,
                            int totalTaskCount);

signals:
    void categoryActivated(int category);
    void taskActivated(const QString &output);

private:
    OverviewPage *overview_ = nullptr;
    SlowTasksPage *slowTasks_ = nullptr;
    TimelinePage *timeline_ = nullptr;
};
