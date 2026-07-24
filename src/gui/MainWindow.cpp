#include "gui/MainWindow.h"

#include "application/AnalysisReportExporter.h"
#include "gui/AnalysisResultsWidget.h"
#include "gui/AppStyle.h"
#include "gui/CategoryPalette.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

using namespace ninja_analyzer;

namespace {

QVector<StepCategory> allCategories()
{
    return {StepCategory::CCompile,
            StepCategory::CxxCompile,
            StepCategory::CudaCompile,
            StepCategory::QtAutogen,
            StepCategory::Resource,
            StepCategory::StaticLink,
            StepCategory::SharedLink,
            StepCategory::ExecutableLink,
            StepCategory::CustomCommand,
            StepCategory::Other};
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Ninja 构建日志分析器"));
    setMinimumSize(820, 600);
    buildInterface();
    setStyleSheet(ninjaAnalyzerStyleSheet());
}

void MainWindow::buildInterface()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralPanel"));
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(24, 18, 24, 20);
    rootLayout->setSpacing(12);

    auto *header = new QFrame(central);
    header->setObjectName(QStringLiteral("headerPanel"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 14, 16, 14);
    auto *brand = new QLabel(QStringLiteral("N"), header);
    brand->setObjectName(QStringLiteral("brandMark"));
    brand->setAlignment(Qt::AlignCenter);
    brand->setFixedSize(42, 42);
    headerLayout->addWidget(brand);
    headerLayout->addSpacing(12);
    auto *titles = new QVBoxLayout;
    auto *title = new QLabel(tr("Ninja 构建分析"), header);
    title->setObjectName(QStringLiteral("windowTitle"));
    auto *subtitle = new QLabel(tr("快速找到拖慢构建的步骤、类型和长尾任务"), header);
    subtitle->setObjectName(QStringLiteral("subtitle"));
    titles->addWidget(title);
    titles->addWidget(subtitle);
    headerLayout->addLayout(titles, 1);
    exportReportButton_ = new QPushButton(tr("导出完整报告"), header);
    exportReportButton_->setObjectName(QStringLiteral("exportReportButton"));
    exportReportButton_->setEnabled(false);
    connect(exportReportButton_, &QPushButton::clicked, this, &MainWindow::exportReport);
    headerLayout->addWidget(exportReportButton_);
    auto *about = new QPushButton(tr("指标说明"), header);
    about->setObjectName(QStringLiteral("ghostButton"));
    connect(about, &QPushButton::clicked, this, &MainWindow::showAbout);
    headerLayout->addWidget(about);
    rootLayout->addWidget(header);

    auto *source = new QFrame(central);
    source->setObjectName(QStringLiteral("card"));
    auto *sourceLayout = new QVBoxLayout(source);
    sourceLayout->setContentsMargins(18, 14, 18, 14);
    auto *sourceHeader = new QHBoxLayout;
    auto *sourceTitle = new QLabel(tr("构建日志"), source);
    sourceTitle->setObjectName(QStringLiteral("sectionTitle"));
    sourceHeader->addWidget(sourceTitle);
    auto *sourceHint = new QLabel(tr("按内容识别 v4 / v5 / v7，文件名不限"), source);
    sourceHint->setObjectName(QStringLiteral("sectionHint"));
    sourceHeader->addWidget(sourceHint);
    sourceHeader->addStretch();
    sourceLayout->addLayout(sourceHeader);
    auto *pathLayout = new QHBoxLayout;
    pathEdit_ = new QLineEdit(source);
    pathEdit_->setObjectName(QStringLiteral("pathEdit"));
    pathEdit_->setPlaceholderText(tr("输入 Ninja 日志文件（任意名称），或包含日志的目录…"));
    pathEdit_->setClearButtonEnabled(true);
    connect(pathEdit_, &QLineEdit::returnPressed, this, [this] { analyzePath(pathEdit_->text()); });
    pathLayout->addWidget(pathEdit_, 1);
    auto *fileButton = new QPushButton(tr("选文件"), source);
    fileButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(fileButton, &QPushButton::clicked, this, &MainWindow::selectLogFile);
    pathLayout->addWidget(fileButton);
    auto *directoryButton = new QPushButton(tr("选目录"), source);
    directoryButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(directoryButton, &QPushButton::clicked, this, &MainWindow::selectDirectory);
    pathLayout->addWidget(directoryButton);
    auto *analyzeButton = new QPushButton(tr("分析日志  →"), source);
    analyzeButton->setObjectName(QStringLiteral("primaryButton"));
    connect(analyzeButton, &QPushButton::clicked, this, [this] { analyzePath(pathEdit_->text()); });
    pathLayout->addWidget(analyzeButton);
    sourceLayout->addLayout(pathLayout);
    diagnosticsLabel_ = new QLabel(source);
    diagnosticsLabel_->setObjectName(QStringLiteral("diagnostics"));
    diagnosticsLabel_->setWordWrap(true);
    diagnosticsLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    diagnosticsLabel_->hide();
    sourceLayout->addWidget(diagnosticsLabel_);
    rootLayout->addWidget(source);

    analysisControls_ = new QFrame(central);
    analysisControls_->setObjectName(QStringLiteral("controlsCard"));
    auto *controlsOuter = new QVBoxLayout(analysisControls_);
    controlsOuter->setContentsMargins(18, 12, 18, 10);
    auto *controls = new QHBoxLayout;
    auto *viewLabel = new QLabel(tr("查看"), analysisControls_);
    viewLabel->setObjectName(QStringLiteral("controlsTitle"));
    controls->addWidget(viewLabel);
    batchCombo_ = new QComboBox(analysisControls_);
    batchCombo_->setObjectName(QStringLiteral("batchCombo"));
    batchCombo_->setMinimumWidth(280);
    connect(batchCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { applySelectedBatch(); });
    controls->addWidget(batchCombo_);
    categoryFilter_ = new QComboBox(analysisControls_);
    categoryFilter_->setObjectName(QStringLiteral("categoryFilter"));
    categoryFilter_->addItem(tr("全部类型"), -1);
    for (const StepCategory category : allCategories()) {
        categoryFilter_->addItem(categoryDisplayName(category), static_cast<int>(category));
    }
    connect(categoryFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { applyFilters(); });
    controls->addWidget(categoryFilter_);
    searchEdit_ = new QLineEdit(analysisControls_);
    searchEdit_->setObjectName(QStringLiteral("outputSearch"));
    searchEdit_->setPlaceholderText(tr("搜索输出路径…"));
    connect(searchEdit_, &QLineEdit::textChanged, this, [this] { applyFilters(); });
    controls->addWidget(searchEdit_, 1);
    controlsOuter->addLayout(controls);
    auto *scopeHint = new QLabel(
        tr("批次决定结论和汇总范围；类型与搜索只筛选“慢任务”和“时间线”。"),
        analysisControls_);
    scopeHint->setObjectName(QStringLiteral("scopeHint"));
    scopeHint->setWordWrap(true);
    controlsOuter->addWidget(scopeHint);
    machineLoadLabel_ = new QLabel(analysisControls_);
    machineLoadLabel_->setObjectName(QStringLiteral("machineLoadContext"));
    machineLoadLabel_->setWordWrap(true);
    machineLoadLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    controlsOuter->addWidget(machineLoadLabel_);
    analysisControls_->hide();
    rootLayout->addWidget(analysisControls_);

    resultFrame_ = new QFrame(central);
    resultFrame_->setObjectName(QStringLiteral("card"));
    auto *resultLayout = new QVBoxLayout(resultFrame_);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    results_ = new AnalysisResultsWidget(resultFrame_);
    connect(results_, &AnalysisResultsWidget::categoryActivated, this, [this](int category) {
        const int index = categoryFilter_->findData(category);
        if (index >= 0) {
            searchEdit_->clear();
            categoryFilter_->setCurrentIndex(index);
            results_->setCurrentIndex(1);
        }
    });
    connect(results_, &AnalysisResultsWidget::taskActivated, this, [this](const QString &output) {
        categoryFilter_->setCurrentIndex(0);
        searchEdit_->setText(output);
        results_->setCurrentIndex(1);
    });
    resultLayout->addWidget(results_);
    resultFrame_->hide();
    rootLayout->addWidget(resultFrame_, 1);

    initialLabel_ = new QLabel(
        tr("选择任意名称的 Ninja 日志，软件会按内容识别 v4 / v5 / v7\n\n"
           "分析在本地完成，不会运行 Ninja 或修改构建目录"),
        central);
    initialLabel_->setObjectName(QStringLiteral("initialState"));
    initialLabel_->setAlignment(Qt::AlignCenter);
    initialLabel_->setWordWrap(true);
    rootLayout->addWidget(initialLabel_, 1);
    setCentralWidget(central);
}

bool MainWindow::analyzePath(const QString &path, bool interactive)
{
    const LocateResult located = analysisService_.locateLogs(path);
    if (!located.ok()) {
        return reportFailure(located.error, interactive);
    }

    QString selectedPath;
    if (located.logPaths.size() == 1) {
        selectedPath = located.logPaths.first();
    } else if (interactive) {
        bool accepted = false;
        selectedPath = QInputDialog::getItem(
            this, tr("选择 Ninja 日志"), tr("目录中发现多个 Ninja 日志，请选择要分析的文件："),
            located.logPaths, 0, false, &accepted);
        if (!accepted || selectedPath.isEmpty()) {
            return false;
        }
    } else {
        return reportFailure(tr("发现多个 Ninja 日志；非交互加载无法代替用户选择。"), false);
    }

    AnalysisLoadResult result = analysisService_.loadLog(selectedPath);
    if (!result.ok()) {
        return reportFailure(result.error, interactive);
    }

    loaded_ = std::move(result.value);
    hasAnalysis_ = true;
    exportReportButton_->setEnabled(true);
    lastError_.clear();
    pathEdit_->setText(loaded_.logPath);
    QStringList details{tr("日志：%1").arg(loaded_.logPath),
                        tr("格式：v%1；忽略 %2 行")
                            .arg(loaded_.logVersion)
                            .arg(loaded_.parseWarnings.size())};
    for (const ParseWarning &warning : loaded_.parseWarnings) {
        details.append(tr("第 %1 行：%2").arg(warning.line).arg(warning.message));
    }
    details.append(loaded_.manifest.warnings);
    diagnosticsLabel_->setToolTip(details.join(QLatin1Char('\n')));
    populateBatchSelector();
    refreshLoadedState();
    return true;
}

void MainWindow::populateBatchSelector()
{
    const QSignalBlocker blocker(batchCombo_);
    batchCombo_->clear();
    batchCombo_->addItem(tr("全部日志记录  ·  %1 个任务").arg(loaded_.records.size()), -1);
    for (int index = 0; index < loaded_.batches.size(); ++index) {
        const BuildBatch &batch = loaded_.batches.at(index);
        const QString name = index == loaded_.batches.size() - 1
            ? tr("最近一次构建（推断）")
            : tr("构建批次 %1（推断）").arg(index + 1);
        batchCombo_->addItem(
            tr("%1  ·  %2 个任务  ·  %3")
                .arg(name)
                .arg(batch.recordCount)
                .arg(analysisService_.formatDuration(batch.maxEndMs - batch.minStartMs)),
            index);
    }
    batchCombo_->setCurrentIndex(batchCombo_->count() - 1);
    applySelectedBatch();
}

void MainWindow::applySelectedBatch()
{
    if (!hasAnalysis_ || batchCombo_->currentIndex() < 0) {
        return;
    }
    currentBatchIndex_ = batchCombo_->currentData().toInt();
    currentRecords_ = analysisService_.recordsForBatch(loaded_, currentBatchIndex_);
    currentAnalysis_ = analysisService_.analyze(currentRecords_);
    refreshLoadedState();
}

void MainWindow::refreshLoadedState()
{
    if (!hasAnalysis_) {
        return;
    }
    int manifestMatches = 0;
    for (const NinjaLogRecord &record : loaded_.records) {
        if (record.classificationSource == ClassificationSource::ManifestRule) {
            ++manifestMatches;
        }
    }
    diagnosticsLabel_->setText(
        tr("✓  已就绪  ·  %1  ·  %2 个有效任务  ·  %3 个构建批次  ·  %4")
            .arg(QFileInfo(loaded_.logPath).fileName())
            .arg(loaded_.records.size())
            .arg(loaded_.batches.size())
            .arg(loaded_.manifest.found()
                     ? tr("规则分类已匹配 %1/%2").arg(manifestMatches).arg(loaded_.records.size())
                     : tr("按输出路径推断分类")));
    diagnosticsLabel_->show();
    const MachineLoadSnapshot &load = loaded_.machineLoad;
    machineLoadLabel_->setText(
        tr("当前机器快照（%1）：%2；%3；%4。\n%5")
            .arg(load.capturedAtUtc.isValid()
                     ? load.capturedAtUtc.toLocalTime().toString(Qt::ISODate)
                     : tr("采集时间不可用"))
            .arg(load.platformDescription())
            .arg(load.logicalProcessorCount > 0
                     ? tr("%1 个逻辑处理器").arg(load.logicalProcessorCount)
                     : tr("逻辑处理器数不可用"))
            .arg(load.loadAverageDescription())
            .arg(MachineLoadSnapshot::limitationText()));
    analysisControls_->show();
    resultFrame_->show();
    initialLabel_->hide();
    results_->setAnalysis(currentAnalysis_);
    applyFilters();
}

void MainWindow::applyFilters()
{
    if (!hasAnalysis_ || !results_) {
        return;
    }
    const int selectedCategory = categoryFilter_->currentData().toInt();
    const QString query = searchEdit_->text().trimmed();
    QVector<NinjaLogRecord> filtered;
    filtered.reserve(currentAnalysis_.slowest.size());
    for (const NinjaLogRecord &record : currentAnalysis_.slowest) {
        if (selectedCategory >= 0 && static_cast<int>(record.category) != selectedCategory) {
            continue;
        }
        if (!query.isEmpty() && !record.output.contains(query, Qt::CaseInsensitive)) {
            continue;
        }
        filtered.append(record);
    }
    filteredRecords_ = std::move(filtered);
    results_->setFilteredRecords(filteredRecords_, currentAnalysis_.summary.taskCount);
}

bool MainWindow::reportFailure(const QString &message, bool interactive)
{
    lastError_ = message;
    if (interactive) {
        QMessageBox::warning(this, tr("无法完成分析"), message);
    }
    return false;
}

void MainWindow::selectLogFile()
{
    const QString selected = QFileDialog::getOpenFileName(
        this, tr("选择 Ninja 日志（按内容识别）"), pathEdit_->text(),
        tr("所有文件 (*);;Ninja 常用日志名 (.ninja_log)"));
    if (!selected.isEmpty()) {
        pathEdit_->setText(selected);
        analyzePath(selected);
    }
}

void MainWindow::selectDirectory()
{
    const QString selected = QFileDialog::getExistingDirectory(
        this, tr("选择构建目录"), pathEdit_->text());
    if (!selected.isEmpty()) {
        pathEdit_->setText(selected);
        analyzePath(selected);
    }
}

void MainWindow::exportReport()
{
    if (!hasAnalysis_) {
        reportFailure(tr("请先成功分析一个 Ninja 日志。"), true);
        return;
    }

    const QFileInfo sourceInfo(loaded_.logPath);
    const QString suggestedPath =
        sourceInfo.dir().filePath(QStringLiteral("ninja-analysis-complete-report.html"));
    QString selected = QFileDialog::getSaveFileName(
        this, tr("导出完整分析报告"), suggestedPath, tr("HTML 报告 (*.html)"));
    if (selected.isEmpty()) {
        return;
    }
    if (QFileInfo(selected).suffix().isEmpty()) {
        selected.append(QStringLiteral(".html"));
    }
    exportReportTo(selected, true);
}

bool MainWindow::exportReportTo(const QString &path, bool interactive)
{
    if (!hasAnalysis_) {
        return reportFailure(tr("请先成功分析一个 Ninja 日志。"), interactive);
    }

    AnalysisReportRequest request;
    request.outputPath = path;
    request.loaded = loaded_;
    request.batchIndex = currentBatchIndex_;
    request.analysis = currentAnalysis_;
    const AnalysisReportResult result = AnalysisReportExporter::exportHtml(request);
    if (!result.ok()) {
        lastError_ = result.error;
        if (interactive) {
            QMessageBox::warning(this, tr("无法导出报告"), result.error);
        }
        return false;
    }

    lastError_.clear();
    lastExportPath_ = result.outputPath;
    if (interactive) {
        QMessageBox::information(
            this, tr("报告导出完成"),
            tr("完整报告已保存到：\n%1").arg(result.outputPath));
    }
    return true;
}

void MainWindow::showAbout()
{
    QMessageBox::information(
        this, tr("关于分析口径"),
        tr("Ninja 日志的开始/结束时间是 Ninja 进程内的相对毫秒。\n\n"
           "• 观察窗口：最早任务开始到最晚任务结束。\n"
           "• 累计任务时间：所有任务耗时之和，并行任务会重复计入。\n"
           "• 平均并行度：累计任务时间 ÷ 观察窗口。\n"
           "• 泳道：为了避免区间遮挡分配的最少显示行，不是 CPU 核心、线程或 worker；"
           "8 核出现 24 条泳道表示有 24 个重叠区间，可能与 -j、I/O 等待和批次推断有关。\n"
           "• 推断批次：日志行的结束时间发生回退时切分；日志被重整后可能不再对应真实构建。\n"
           "• 机器负载：显示加载日志时当前分析机器的快照，不是构建时历史负载或 CPU 使用率。\n"
           "• 瓶颈提示：只描述日志可证明的耗时和重叠现象，不是 CPU/I/O 根因，也不是严格关键路径。\n\n"
           "本软件只读本地日志和 build.ninja，不运行构建、不修改文件、不上传数据。"));
}
