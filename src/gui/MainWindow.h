#pragma once

#include "application/AnalysisService.h"

#include <QMainWindow>

class AnalysisResultsWidget;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool analyzePath(const QString &path, bool interactive = true);
    bool exportReportTo(const QString &path, bool interactive = true);
    bool hasLoadedAnalysis() const { return hasAnalysis_; }
    QString currentLogPath() const { return loaded_.logPath; }
    QString lastError() const { return lastError_; }
    int currentTaskCount() const { return currentAnalysis_.summary.taskCount; }
    int inferredBatchCount() const { return loaded_.batches.size(); }
    int filteredTaskCount() const { return filteredRecords_.size(); }
    QString lastExportPath() const { return lastExportPath_; }

private:
    void buildInterface();
    void selectLogFile();
    void selectDirectory();
    void exportReport();
    void showAbout();
    void populateBatchSelector();
    void applySelectedBatch();
    void refreshLoadedState();
    void applyFilters();
    bool reportFailure(const QString &message, bool interactive);

    QLineEdit *pathEdit_ = nullptr;
    QComboBox *batchCombo_ = nullptr;
    QComboBox *categoryFilter_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QLabel *diagnosticsLabel_ = nullptr;
    QLabel *machineLoadLabel_ = nullptr;
    QLabel *initialLabel_ = nullptr;
    QFrame *analysisControls_ = nullptr;
    QFrame *resultFrame_ = nullptr;
    AnalysisResultsWidget *results_ = nullptr;
    QPushButton *exportReportButton_ = nullptr;

    ninja_analyzer::AnalysisService analysisService_;
    ninja_analyzer::LoadedAnalysis loaded_;
    QVector<ninja_analyzer::NinjaLogRecord> currentRecords_;
    QVector<ninja_analyzer::NinjaLogRecord> filteredRecords_;
    ninja_analyzer::AnalysisResult currentAnalysis_;
    int currentBatchIndex_ = -1;
    bool hasAnalysis_ = false;
    QString lastError_;
    QString lastExportPath_;
};
