#pragma once

#include "core/NinjaLogTypes.h"

#include <QScrollArea>

class QLabel;
class QListWidget;
class OverviewChartsWidget;
class QTableWidget;

class OverviewPage final : public QScrollArea {
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = nullptr);
    void setAnalysis(const ninja_analyzer::AnalysisResult &analysis);

signals:
    void categoryActivated(int category);
    void taskActivated(const QString &output);
    void showSlowTasksRequested();
    void showTimelineRequested();

private:
    QLabel *conclusionTitleLabel_ = nullptr;
    QLabel *conclusionDetailLabel_ = nullptr;
    QLabel *taskCountValue_ = nullptr;
    QLabel *observedSpanValue_ = nullptr;
    QLabel *totalTaskValue_ = nullptr;
    QLabel *averageParallelValue_ = nullptr;
    QLabel *maximumParallelValue_ = nullptr;
    QTableWidget *categoryTable_ = nullptr;
    OverviewChartsWidget *overviewCharts_ = nullptr;
    QListWidget *insightsList_ = nullptr;
};
