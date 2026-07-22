#pragma once

#include "core/NinjaLogTypes.h"

#include <QMainWindow>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class OverviewChartsWidget;
class QPushButton;
class QTableView;
class QTableWidget;
class QTabWidget;
class SlowTasksModel;
class TimelineWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool analyzePath(const QString &path, bool interactive = true);
    bool hasLoadedAnalysis() const { return hasAnalysis_; }
    QString currentLogPath() const { return currentLogPath_; }
    QString lastError() const { return lastError_; }
    int currentTaskCount() const { return currentAnalysis_.summary.taskCount; }
    int inferredBatchCount() const { return batches_.size(); }
    int filteredTaskCount() const { return filteredRecords_.size(); }

private:
    void buildInterface();
    void applyStyle();
    void selectLogFile();
    void selectDirectory();
    void showAbout();
    void populateBatchSelector();
    void applySelectedBatch();
    void refreshLoadedState();
    void refreshAnalysisViews();
    void applyFilters();
    bool reportFailure(const QString &message, bool interactive);

    QLineEdit *pathEdit_ = nullptr;
    QPushButton *analyzeButton_ = nullptr;
    QComboBox *batchCombo_ = nullptr;
    QLabel *diagnosticsLabel_ = nullptr;
    QLabel *initialLabel_ = nullptr;
    QLabel *conclusionTitleLabel_ = nullptr;
    QLabel *conclusionDetailLabel_ = nullptr;
    QLabel *taskCountValue_ = nullptr;
    QLabel *observedSpanValue_ = nullptr;
    QLabel *totalTaskValue_ = nullptr;
    QLabel *averageParallelValue_ = nullptr;
    QLabel *maximumParallelValue_ = nullptr;
    QLabel *filterStatusLabel_ = nullptr;
    QLabel *timelineStatusLabel_ = nullptr;
    QComboBox *categoryFilter_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QTableWidget *categoryTable_ = nullptr;
    OverviewChartsWidget *overviewCharts_ = nullptr;
    QListWidget *insightsList_ = nullptr;
    QTableView *slowTasksView_ = nullptr;
    SlowTasksModel *slowTasksModel_ = nullptr;
    TimelineWidget *timelineWidget_ = nullptr;
    QTabWidget *resultTabs_ = nullptr;
    QFrame *analysisControls_ = nullptr;
    QFrame *resultFrame_ = nullptr;

    bool hasAnalysis_ = false;
    QString currentLogPath_;
    QString lastError_;
    int logVersion_ = 0;
    int ignoredLineCount_ = 0;
    ninja_analyzer::ManifestInfo manifest_;
    QVector<ninja_analyzer::NinjaLogRecord> allRecords_;
    QVector<ninja_analyzer::NinjaLogRecord> currentRecords_;
    QVector<ninja_analyzer::NinjaLogRecord> filteredRecords_;
    QVector<ninja_analyzer::BuildBatch> batches_;
    ninja_analyzer::AnalysisResult currentAnalysis_;
};
