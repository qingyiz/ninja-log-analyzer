#include "gui/MainWindow.h"

#include "core/BuildAnalyzer.h"
#include "core/LogLocator.h"
#include "core/NinjaLogParser.h"
#include "core/NinjaManifestParser.h"
#include "gui/CategoryPalette.h"
#include "gui/OverviewChartsWidget.h"
#include "gui/SlowTasksModel.h"
#include "gui/TimelineWidget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
#include <QTableView>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

using namespace ninja_analyzer;

namespace {

QFrame *createMetricCard(QWidget *parent,
                         const QString &caption,
                         const QString &valueObjectName,
                         const QString &hint,
                         const QString &tone,
                         QLabel **valueLabel)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("metricCard"));
    card->setProperty("tone", tone);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 13);
    layout->setSpacing(4);
    auto *captionLabel = new QLabel(caption, card);
    captionLabel->setObjectName(QStringLiteral("metricCaption"));
    auto *value = new QLabel(QStringLiteral("—"), card);
    value->setObjectName(QStringLiteral("metricValue"));
    value->setProperty("testName", valueObjectName);
    value->setAccessibleName(valueObjectName);
    value->setObjectName(valueObjectName);
    layout->addWidget(captionLabel);
    layout->addWidget(value);
    auto *hintLabel = new QLabel(hint, card);
    hintLabel->setObjectName(QStringLiteral("metricHint"));
    layout->addWidget(hintLabel);
    *valueLabel = value;
    return card;
}

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

QTableWidgetItem *textItem(const QString &text, Qt::Alignment alignment = Qt::AlignLeft)
{
    auto *item = new QTableWidgetItem(text);
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    item->setTextAlignment(alignment | Qt::AlignVCenter);
#else
    item->setTextAlignment(static_cast<int>(alignment | Qt::AlignVCenter));
#endif
    return item;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Ninja 构建日志分析器"));
    setMinimumSize(820, 600);
    buildInterface();
    applyStyle();
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

    auto *brandMark = new QLabel(QStringLiteral("N"), header);
    brandMark->setObjectName(QStringLiteral("brandMark"));
    brandMark->setAlignment(Qt::AlignCenter);
    brandMark->setFixedSize(42, 42);
    headerLayout->addWidget(brandMark);
    headerLayout->addSpacing(12);

    auto *titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(2);
    auto *title = new QLabel(tr("Ninja 构建分析"), header);
    title->setObjectName(QStringLiteral("windowTitle"));
    auto *subtitle = new QLabel(
        tr("快速找到拖慢构建的步骤、类型和长尾任务"), header);
    subtitle->setObjectName(QStringLiteral("subtitle"));
    titleColumn->addWidget(title);
    titleColumn->addWidget(subtitle);
    headerLayout->addLayout(titleColumn, 1);

    auto *aboutButton = new QPushButton(tr("指标说明"), header);
    aboutButton->setObjectName(QStringLiteral("ghostButton"));
    aboutButton->setCursor(Qt::PointingHandCursor);
    connect(aboutButton, &QPushButton::clicked, this, &MainWindow::showAbout);
    headerLayout->addWidget(aboutButton, 0, Qt::AlignVCenter);
    rootLayout->addWidget(header);

    auto *sourceCard = new QFrame(central);
    sourceCard->setObjectName(QStringLiteral("card"));
    auto *sourceLayout = new QVBoxLayout(sourceCard);
    sourceLayout->setContentsMargins(18, 14, 18, 14);
    sourceLayout->setSpacing(9);
    auto *sourceHeader = new QHBoxLayout;
    auto *sourceTitle = new QLabel(tr("构建日志"), sourceCard);
    sourceTitle->setObjectName(QStringLiteral("sectionTitle"));
    sourceHeader->addWidget(sourceTitle);
    auto *sourceHint = new QLabel(tr("支持 .ninja_log 或构建目录"), sourceCard);
    sourceHint->setObjectName(QStringLiteral("sectionHint"));
    sourceHeader->addWidget(sourceHint);
    sourceHeader->addStretch();
    sourceLayout->addLayout(sourceHeader);

    auto *pathLayout = new QHBoxLayout;
    pathLayout->setSpacing(10);
    pathEdit_ = new QLineEdit(sourceCard);
    pathEdit_->setObjectName(QStringLiteral("pathEdit"));
    pathEdit_->setPlaceholderText(tr("输入 .ninja_log 文件，或包含它的构建目录…"));
    pathEdit_->setClearButtonEnabled(true);
    connect(pathEdit_, &QLineEdit::returnPressed, this, [this] {
        analyzePath(pathEdit_->text());
    });
    pathLayout->addWidget(pathEdit_, 1);

    auto *fileButton = new QPushButton(tr("选文件"), sourceCard);
    fileButton->setObjectName(QStringLiteral("secondaryButton"));
    fileButton->setCursor(Qt::PointingHandCursor);
    connect(fileButton, &QPushButton::clicked, this, &MainWindow::selectLogFile);
    pathLayout->addWidget(fileButton);

    auto *directoryButton = new QPushButton(tr("选目录"), sourceCard);
    directoryButton->setObjectName(QStringLiteral("secondaryButton"));
    directoryButton->setCursor(Qt::PointingHandCursor);
    connect(directoryButton, &QPushButton::clicked, this, &MainWindow::selectDirectory);
    pathLayout->addWidget(directoryButton);

    analyzeButton_ = new QPushButton(tr("分析日志  →"), sourceCard);
    analyzeButton_->setObjectName(QStringLiteral("primaryButton"));
    analyzeButton_->setCursor(Qt::PointingHandCursor);
    connect(analyzeButton_, &QPushButton::clicked, this, [this] {
        analyzePath(pathEdit_->text());
    });
    pathLayout->addWidget(analyzeButton_);
    sourceLayout->addLayout(pathLayout);

    diagnosticsLabel_ = new QLabel(sourceCard);
    diagnosticsLabel_->setObjectName(QStringLiteral("diagnostics"));
    diagnosticsLabel_->setWordWrap(true);
    diagnosticsLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    diagnosticsLabel_->hide();
    sourceLayout->addWidget(diagnosticsLabel_);
    rootLayout->addWidget(sourceCard);

    analysisControls_ = new QFrame(central);
    analysisControls_->setObjectName(QStringLiteral("controlsCard"));
    auto *controlsOuter = new QVBoxLayout(analysisControls_);
    controlsOuter->setContentsMargins(18, 12, 18, 10);
    controlsOuter->setSpacing(6);
    auto *controlsLayout = new QHBoxLayout;
    controlsLayout->setSpacing(9);
    auto *viewLabel = new QLabel(tr("查看"), analysisControls_);
    viewLabel->setObjectName(QStringLiteral("controlsTitle"));
    controlsLayout->addWidget(viewLabel);
    batchCombo_ = new QComboBox(analysisControls_);
    batchCombo_->setObjectName(QStringLiteral("batchCombo"));
    batchCombo_->setMinimumWidth(280);
    connect(batchCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { applySelectedBatch(); });
    controlsLayout->addWidget(batchCombo_);

    categoryFilter_ = new QComboBox(analysisControls_);
    categoryFilter_->setObjectName(QStringLiteral("categoryFilter"));
    categoryFilter_->addItem(tr("全部类型"), -1);
    for (const StepCategory category : allCategories()) {
        categoryFilter_->addItem(categoryDisplayName(category), static_cast<int>(category));
    }
    connect(categoryFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { applyFilters(); });
    controlsLayout->addWidget(categoryFilter_);

    searchEdit_ = new QLineEdit(analysisControls_);
    searchEdit_->setObjectName(QStringLiteral("outputSearch"));
    searchEdit_->setPlaceholderText(tr("搜索输出路径…"));
    searchEdit_->setMinimumWidth(190);
    connect(searchEdit_, &QLineEdit::textChanged, this, [this] { applyFilters(); });
    controlsLayout->addWidget(searchEdit_, 1);
    controlsLayout->addStretch();
    controlsOuter->addLayout(controlsLayout);
    auto *scopeHint = new QLabel(
        tr("批次决定上方结论和汇总范围；类型与搜索只筛选“慢任务”和“时间线”。"),
        analysisControls_);
    scopeHint->setObjectName(QStringLiteral("scopeHint"));
    scopeHint->setWordWrap(true);
    controlsOuter->addWidget(scopeHint);
    analysisControls_->hide();
    rootLayout->addWidget(analysisControls_);

    resultFrame_ = new QFrame(central);
    resultFrame_->setObjectName(QStringLiteral("card"));
    auto *resultLayout = new QVBoxLayout(resultFrame_);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    resultTabs_ = new QTabWidget(resultFrame_);
    resultTabs_->setObjectName(QStringLiteral("resultTabs"));
    resultTabs_->tabBar()->setExpanding(false);
    resultTabs_->tabBar()->setUsesScrollButtons(false);

    auto *overviewScroll = new QScrollArea(resultTabs_);
    overviewScroll->setWidgetResizable(true);
    overviewScroll->setFrameShape(QFrame::NoFrame);
    auto *overview = new QWidget(overviewScroll);
    auto *overviewLayout = new QVBoxLayout(overview);
    overviewLayout->setContentsMargins(16, 16, 16, 16);
    overviewLayout->setSpacing(12);

    auto *conclusionPanel = new QFrame(overview);
    conclusionPanel->setObjectName(QStringLiteral("conclusionPanel"));
    auto *conclusionLayout = new QHBoxLayout(conclusionPanel);
    conclusionLayout->setContentsMargins(20, 16, 14, 16);
    conclusionLayout->setSpacing(18);
    auto *conclusionCopy = new QVBoxLayout;
    conclusionCopy->setSpacing(5);
    auto *conclusionEyebrow = new QLabel(tr("分析结论"), conclusionPanel);
    conclusionEyebrow->setObjectName(QStringLiteral("conclusionEyebrow"));
    conclusionTitleLabel_ = new QLabel(conclusionPanel);
    conclusionTitleLabel_->setObjectName(QStringLiteral("conclusionTitle"));
    conclusionTitleLabel_->setWordWrap(true);
    conclusionDetailLabel_ = new QLabel(conclusionPanel);
    conclusionDetailLabel_->setObjectName(QStringLiteral("conclusionDetail"));
    conclusionDetailLabel_->setWordWrap(true);
    conclusionCopy->addWidget(conclusionEyebrow);
    conclusionCopy->addWidget(conclusionTitleLabel_);
    conclusionCopy->addWidget(conclusionDetailLabel_);
    conclusionLayout->addLayout(conclusionCopy, 1);

    auto *inspectSlowButton = new QPushButton(tr("查看最慢任务"), conclusionPanel);
    inspectSlowButton->setObjectName(QStringLiteral("inlineButton"));
    inspectSlowButton->setCursor(Qt::PointingHandCursor);
    connect(inspectSlowButton, &QPushButton::clicked, this, [this] {
        resultTabs_->setCurrentIndex(1);
    });
    conclusionLayout->addWidget(inspectSlowButton, 0, Qt::AlignVCenter);

    auto *inspectTimelineButton = new QPushButton(tr("打开时间线  →"), conclusionPanel);
    inspectTimelineButton->setObjectName(QStringLiteral("inlinePrimaryButton"));
    inspectTimelineButton->setCursor(Qt::PointingHandCursor);
    connect(inspectTimelineButton, &QPushButton::clicked, this, [this] {
        resultTabs_->setCurrentIndex(2);
    });
    conclusionLayout->addWidget(inspectTimelineButton, 0, Qt::AlignVCenter);
    overviewLayout->addWidget(conclusionPanel);

    auto *metricsLayout = new QHBoxLayout;
    metricsLayout->setSpacing(10);
    metricsLayout->addWidget(createMetricCard(
        overview, tr("构建耗时"), QStringLiteral("summarySpanValue"),
        tr("从首个任务到最后结束"), QStringLiteral("primary"), &observedSpanValue_), 2);
    metricsLayout->addWidget(createMetricCard(
        overview, tr("任务数量"), QStringLiteral("summaryTaskValue"),
        tr("当前批次内的构建步骤"), QStringLiteral("neutral"), &taskCountValue_));
    metricsLayout->addWidget(createMetricCard(
        overview, tr("累计工作量"), QStringLiteral("summaryTotalValue"),
        tr("并行任务会重复计入"), QStringLiteral("neutral"), &totalTaskValue_));
    metricsLayout->addWidget(createMetricCard(
        overview, tr("平均并行"), QStringLiteral("summaryAverageValue"),
        tr("任务重叠的平均水平"), QStringLiteral("success"), &averageParallelValue_));
    metricsLayout->addWidget(createMetricCard(
        overview, tr("峰值并行"), QStringLiteral("summaryMaximumValue"),
        tr("同时运行任务的峰值"), QStringLiteral("success"), &maximumParallelValue_));
    overviewLayout->addLayout(metricsLayout);

    auto *overviewSplitter = new QSplitter(Qt::Horizontal, overview);
    auto *categoryPanel = new QFrame(overviewSplitter);
    categoryPanel->setObjectName(QStringLiteral("innerPanel"));
    auto *categoryLayout = new QVBoxLayout(categoryPanel);
    categoryLayout->setContentsMargins(16, 14, 16, 16);
    auto *categoryHeader = new QHBoxLayout;
    auto *categoryTitle = new QLabel(tr("耗时都花在哪里"), categoryPanel);
    categoryTitle->setObjectName(QStringLiteral("panelTitle"));
    categoryHeader->addWidget(categoryTitle);
    categoryHeader->addStretch();
    auto *dataTableToggle = new QPushButton(tr("查看数据表"), categoryPanel);
    dataTableToggle->setObjectName(QStringLiteral("compactButton"));
    dataTableToggle->setCheckable(true);
    dataTableToggle->setCursor(Qt::PointingHandCursor);
    categoryHeader->addWidget(dataTableToggle);
    categoryLayout->addLayout(categoryHeader);
    auto *categorySubtitle = new QLabel(tr("按累计任务时间排序；点击任一类型查看相关慢任务"), categoryPanel);
    categorySubtitle->setObjectName(QStringLiteral("panelSubtitle"));
    categoryLayout->addWidget(categorySubtitle);

    overviewCharts_ = new OverviewChartsWidget(categoryPanel);
    overviewCharts_->setObjectName(QStringLiteral("overviewCharts"));
    connect(overviewCharts_, &OverviewChartsWidget::categoryActivated, this,
            [this](int category) {
                const int filterIndex = categoryFilter_->findData(category);
                if (filterIndex >= 0) {
                    searchEdit_->clear();
                    categoryFilter_->setCurrentIndex(filterIndex);
                    resultTabs_->setCurrentIndex(1);
                }
            });
    connect(overviewCharts_, &OverviewChartsWidget::taskActivated, this,
            [this](const QString &output) {
                categoryFilter_->setCurrentIndex(0);
                searchEdit_->setText(output);
                resultTabs_->setCurrentIndex(1);
            });
    categoryLayout->addWidget(overviewCharts_, 1);

    categoryTable_ = new QTableWidget(categoryPanel);
    categoryTable_->setObjectName(QStringLiteral("categoryTable"));
    categoryTable_->setColumnCount(6);
    categoryTable_->setHorizontalHeaderLabels(
        {tr("类型"), tr("任务数"), tr("累计"), tr("平均"), tr("最长"), tr("工作量占比")});
    categoryTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    categoryTable_->setSelectionMode(QAbstractItemView::NoSelection);
    categoryTable_->setAlternatingRowColors(true);
    categoryTable_->verticalHeader()->hide();
    categoryTable_->verticalHeader()->setDefaultSectionSize(36);
    categoryTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 6; ++column) {
        categoryTable_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    categoryTable_->viewport()->setCursor(Qt::PointingHandCursor);
    connect(categoryTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QTableWidgetItem *categoryItem = categoryTable_->item(row, 0);
        if (!categoryItem) {
            return;
        }
        const int filterIndex = categoryFilter_->findData(categoryItem->data(Qt::UserRole));
        if (filterIndex >= 0) {
            categoryFilter_->setCurrentIndex(filterIndex);
            resultTabs_->setCurrentIndex(1);
        }
    });
    categoryLayout->addWidget(categoryTable_);
    categoryTable_->hide();
    connect(dataTableToggle, &QPushButton::toggled, this,
            [this, dataTableToggle](bool checked) {
                categoryTable_->setVisible(checked);
                dataTableToggle->setText(checked ? tr("隐藏数据表") : tr("查看数据表"));
            });

    auto *insightsPanel = new QFrame(overviewSplitter);
    insightsPanel->setObjectName(QStringLiteral("innerPanel"));
    auto *insightsLayout = new QVBoxLayout(insightsPanel);
    insightsLayout->setContentsMargins(16, 14, 16, 16);
    auto *insightsTitle = new QLabel(tr("建议先看这些"), insightsPanel);
    insightsTitle->setObjectName(QStringLiteral("panelTitle"));
    insightsLayout->addWidget(insightsTitle);
    auto *insightsSubtitle = new QLabel(tr("根据耗时、并行和结束位置自动生成"), insightsPanel);
    insightsSubtitle->setObjectName(QStringLiteral("panelSubtitle"));
    insightsLayout->addWidget(insightsSubtitle);
    insightsList_ = new QListWidget(insightsPanel);
    insightsList_->setObjectName(QStringLiteral("insightsList"));
    insightsList_->setWordWrap(true);
    insightsList_->setSpacing(5);
    insightsList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    insightsList_->setSelectionMode(QAbstractItemView::NoSelection);
    insightsLayout->addWidget(insightsList_);
    overviewSplitter->addWidget(categoryPanel);
    overviewSplitter->addWidget(insightsPanel);
    overviewSplitter->setStretchFactor(0, 3);
    overviewSplitter->setStretchFactor(1, 2);
    overviewLayout->addWidget(overviewSplitter, 1);
    overviewScroll->setWidget(overview);
    resultTabs_->addTab(overviewScroll, tr("概览"));

    auto *slowTab = new QWidget(resultTabs_);
    auto *slowLayout = new QVBoxLayout(slowTab);
    slowLayout->setContentsMargins(12, 12, 12, 12);
    filterStatusLabel_ = new QLabel(slowTab);
    filterStatusLabel_->setObjectName(QStringLiteral("filterStatus"));
    slowLayout->addWidget(filterStatusLabel_);
    slowTasksModel_ = new SlowTasksModel(this);
    slowTasksView_ = new QTableView(slowTab);
    slowTasksView_->setObjectName(QStringLiteral("slowTasksView"));
    slowTasksView_->setModel(slowTasksModel_);
    slowTasksView_->setAlternatingRowColors(true);
    slowTasksView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    slowTasksView_->setSelectionMode(QAbstractItemView::SingleSelection);
    slowTasksView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    slowTasksView_->setTextElideMode(Qt::ElideMiddle);
    slowTasksView_->verticalHeader()->setDefaultSectionSize(30);
    slowTasksView_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    slowTasksView_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    slowLayout->addWidget(slowTasksView_, 1);
    resultTabs_->addTab(slowTab, tr("最慢步骤"));

    auto *timelineTab = new QWidget(resultTabs_);
    auto *timelineLayout = new QVBoxLayout(timelineTab);
    timelineLayout->setContentsMargins(12, 12, 12, 12);
    timelineStatusLabel_ = new QLabel(timelineTab);
    timelineStatusLabel_->setObjectName(QStringLiteral("timelineStatus"));
    timelineLayout->addWidget(timelineStatusLabel_);
    auto *timelineScroll = new QScrollArea(timelineTab);
    timelineScroll->setObjectName(QStringLiteral("timelineScroll"));
    timelineScroll->setWidgetResizable(true);
    timelineScroll->setFrameShape(QFrame::NoFrame);
    timelineWidget_ = new TimelineWidget(timelineScroll);
    timelineWidget_->setObjectName(QStringLiteral("timelineWidget"));
    timelineScroll->setWidget(timelineWidget_);
    timelineLayout->addWidget(timelineScroll, 1);
    resultTabs_->addTab(timelineTab, tr("并发时间线"));

    resultLayout->addWidget(resultTabs_);
    resultFrame_->hide();
    rootLayout->addWidget(resultFrame_, 1);

    initialLabel_ = new QLabel(
        tr("把 .ninja_log 拖到这里，或从上方选择文件\n\n分析在本地完成，不会运行 Ninja 或修改构建目录"),
        central);
    initialLabel_->setObjectName(QStringLiteral("initialState"));
    initialLabel_->setAlignment(Qt::AlignCenter);
    initialLabel_->setWordWrap(true);
    rootLayout->addWidget(initialLabel_, 1);

    setCentralWidget(central);
}

void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QWidget#centralPanel { background: #F5F6FA; color: #1D2433; }
        QFrame#headerPanel {
            background: #FFFFFF;
            border: 1px solid #E3E6EE;
            border-radius: 14px;
        }
        QLabel#brandMark {
            color: #FFFFFF; background: #5B5BD6; border: 0; border-radius: 11px;
            font-size: 21px; font-weight: 800;
        }
        QLabel#windowTitle { color: #171A2B; font-size: 18px; font-weight: 750; }
        QLabel#subtitle { color: #737B8C; font-size: 12px; }
        QFrame#card, QFrame#controlsCard {
            background: #FFFFFF;
            border: 1px solid #E3E6EE;
            border-radius: 12px;
        }
        QLabel#sectionTitle, QLabel#controlsTitle {
            color: #252A3A; font-size: 12px; font-weight: 750;
        }
        QLabel#sectionHint { color: #9097A6; font-size: 11px; }
        QLineEdit, QComboBox {
            min-height: 36px; padding: 0 11px;
            background: #FAFAFC; color: #292E3D;
            border: 1px solid #D9DDE7;
            border-radius: 8px;
            selection-background-color: #5B5BD6;
        }
        QLineEdit:focus, QComboBox:focus { border: 1px solid #6565DF; background: #FFFFFF; }
        QPushButton { min-height: 36px; padding: 0 14px; border-radius: 8px; font-weight: 650; }
        QPushButton#primaryButton {
            color: white; background: #5B5BD6; border: 1px solid #5B5BD6;
            padding: 0 18px;
        }
        QPushButton#primaryButton:hover { background: #4848C4; }
        QPushButton#secondaryButton {
            color: #454B5D; background: #FFFFFF; border: 1px solid #D9DDE7;
        }
        QPushButton#secondaryButton:hover { border-color: #8C8CE8; color: #4E4EC7; }
        QPushButton#ghostButton {
            color: #5C6373; background: #F7F7FA; border: 1px solid #E2E4EB;
        }
        QPushButton#ghostButton:hover { color: #4E4EC7; border-color: #B9B9ED; }
        QLabel#diagnostics {
            color: #3E665B; background: #F0F9F5; border: 1px solid #D1EDE1;
            border-radius: 8px;
            padding: 8px 11px;
        }
        QLabel#scopeHint { color: #858C9B; font-size: 11px; }
        QLabel#initialState { color: #838A9A; font-size: 14px; line-height: 1.5; }
        QTabWidget::pane { border: 0; background: #FFFFFF; }
        QTabBar::tab {
            color: #747B8C; background: transparent; border: 0;
            padding: 13px 19px 11px 19px; margin-right: 2px; font-weight: 650;
        }
        QTabBar::tab:hover { color: #4E4EC7; }
        QTabBar::tab:selected { color: #4E4EC7; border-bottom: 3px solid #5B5BD6; }
        QFrame#conclusionPanel {
            background: #20223A; border: 1px solid #292C49; border-radius: 12px;
        }
        QLabel#conclusionEyebrow {
            color: #A9ACFF; font-size: 10px; font-weight: 750; letter-spacing: 1px;
        }
        QLabel#conclusionTitle { color: #FFFFFF; font-size: 17px; font-weight: 750; }
        QLabel#conclusionDetail { color: #BFC3D8; font-size: 11px; }
        QPushButton#inlineButton {
            color: #D7D9E7; background: #2B2E4A; border: 1px solid #414562;
        }
        QPushButton#inlineButton:hover { background: #353855; color: #FFFFFF; }
        QPushButton#inlinePrimaryButton {
            color: #20223A; background: #B9BBFF; border: 1px solid #B9BBFF;
        }
        QPushButton#inlinePrimaryButton:hover { background: #D0D1FF; }
        QPushButton#compactButton {
            min-height: 26px; padding: 0 10px; color: #646B7B;
            background: #FFFFFF; border: 1px solid #DEE1E8; font-size: 10px;
        }
        QPushButton#compactButton:hover { color: #4E4EC7; border-color: #B9B9ED; }
        QPushButton#compactButton:checked { color: #4E4EC7; background: #F0F0FF; }
        QWidget#overviewCharts { background: transparent; }
        QFrame#metricCard {
            background: #FAFAFC; border: 1px solid #E5E7EE; border-radius: 10px;
        }
        QFrame#metricCard[tone="primary"] {
            background: #F0F0FF; border: 1px solid #D7D7FA;
        }
        QFrame#metricCard[tone="success"] { background: #F4FAF7; border: 1px solid #DDEEE6; }
        QFrame#innerPanel {
            background: #FAFAFC; border: 1px solid #E5E7EE; border-radius: 10px;
        }
        QFrame#metricCard QLabel { border: 0; background: transparent; }
        QLabel#metricCaption { color: #6F7687; font-size: 11px; font-weight: 650; }
        QLabel#metricHint { color: #9A9FAD; font-size: 10px; }
        QLabel#summaryTaskValue, QLabel#summarySpanValue, QLabel#summaryTotalValue,
        QLabel#summaryAverageValue, QLabel#summaryMaximumValue {
            color: #202435; font-size: 21px; font-weight: 780;
        }
        QLabel#summarySpanValue { color: #4E4EC7; font-size: 24px; }
        QLabel#filterStatus, QLabel#timelineStatus {
            color: #747B8C; font-size: 11px;
        }
        QLabel#panelTitle { color: #252A3A; font-size: 13px; font-weight: 750; }
        QLabel#panelSubtitle { color: #8B92A1; font-size: 10px; padding-bottom: 4px; }
        QTableView, QTableWidget, QListWidget {
            background: #FFFFFF; alternate-background-color: #FAFAFC;
            border: 1px solid #E4E7ED; border-radius: 8px;
            gridline-color: #ECEEF3; color: #3D4353;
        }
        QTableView::item:hover, QTableWidget::item:hover { background: #F0F0FF; }
        QListWidget#insightsList { background: transparent; border: 0; outline: 0; }
        QListWidget#insightsList::item {
            background: #FFFFFF; border: 1px solid #E4E7ED; border-radius: 8px;
            padding: 10px 11px; margin-bottom: 3px;
        }
        QHeaderView::section {
            color: #6E7585; background: #F5F6F9; border: 0;
            border-bottom: 1px solid #E0E3EA; padding: 8px; font-weight: 650;
        }
        QProgressBar {
            min-width: 110px; border: 0; border-radius: 5px;
            background: #ECEEF3; color: #3D4353; text-align: center;
        }
        QProgressBar::chunk { background: #7575DE; border-radius: 5px; }
        QSplitter::handle { background: transparent; width: 8px; }
        QScrollBar:vertical { background: transparent; width: 9px; margin: 2px; }
        QScrollBar::handle:vertical { background: #CDD1DA; border-radius: 4px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )"));
}

bool MainWindow::analyzePath(const QString &path, bool interactive)
{
    const LocateResult located = LogLocator::resolve(path);
    if (!located.ok()) {
        return reportFailure(located.error, interactive);
    }

    QString selectedPath;
    if (located.logPaths.size() == 1) {
        selectedPath = located.logPaths.first();
    } else if (interactive) {
        bool accepted = false;
        selectedPath = QInputDialog::getItem(
            this,
            tr("选择 Ninja 日志"),
            tr("目录中发现多个 .ninja_log，请选择要分析的文件："),
            located.logPaths,
            0,
            false,
            &accepted);
        if (!accepted || selectedPath.isEmpty()) {
            return false;
        }
    } else {
        return reportFailure(
            tr("发现多个 .ninja_log；非交互加载无法代替用户选择。"), false);
    }

    ParseResult parsed = NinjaLogParser::parse(selectedPath);
    if (!parsed.ok()) {
        return reportFailure(parsed.fatalError, interactive);
    }

    ManifestInfo manifest = NinjaManifestParser::loadNear(selectedPath);
    QVector<NinjaLogRecord> records = parsed.records;
    NinjaManifestParser::enrichRecords(records, manifest);
    QVector<BuildBatch> batches = BuildAnalyzer::partitionBatches(records);
    if (batches.isEmpty()) {
        return reportFailure(tr("日志没有形成可分析的记录批次。"), interactive);
    }

    // Commit only after the complete candidate pipeline succeeded.
    currentLogPath_ = QFileInfo(selectedPath).canonicalFilePath();
    if (currentLogPath_.isEmpty()) {
        currentLogPath_ = QFileInfo(selectedPath).absoluteFilePath();
    }
    logVersion_ = parsed.version;
    ignoredLineCount_ = parsed.warnings.size();
    manifest_ = std::move(manifest);
    allRecords_ = std::move(records);
    batches_ = std::move(batches);
    hasAnalysis_ = true;
    lastError_.clear();
    pathEdit_->setText(currentLogPath_);

    QStringList warningDetails{
        tr("日志：%1").arg(currentLogPath_),
        tr("格式：v%1；忽略 %2 行").arg(logVersion_).arg(ignoredLineCount_)};
    for (const ParseWarning &warning : parsed.warnings) {
        warningDetails.append(tr("第 %1 行：%2").arg(warning.line).arg(warning.message));
    }
    warningDetails.append(manifest_.warnings);
    diagnosticsLabel_->setToolTip(warningDetails.join(QLatin1Char('\n')));

    populateBatchSelector();
    refreshLoadedState();
    return true;
}

void MainWindow::populateBatchSelector()
{
    const QSignalBlocker blocker(batchCombo_);
    batchCombo_->clear();
    batchCombo_->addItem(tr("全部日志记录  ·  %1 个任务").arg(allRecords_.size()), -1);
    for (int index = 0; index < batches_.size(); ++index) {
        const BuildBatch &batch = batches_.at(index);
        const QString batchName = index == batches_.size() - 1
            ? tr("最近一次构建（推断）")
            : tr("构建批次 %1（推断）").arg(index + 1);
        batchCombo_->addItem(
            tr("%1  ·  %2 个任务  ·  %3")
                .arg(batchName)
                .arg(batch.recordCount)
                .arg(BuildAnalyzer::formatDuration(batch.maxEndMs - batch.minStartMs)), index);
    }
    batchCombo_->setCurrentIndex(batchCombo_->count() - 1);
    applySelectedBatch();
}

void MainWindow::applySelectedBatch()
{
    if (!hasAnalysis_ || batchCombo_->currentIndex() < 0) {
        return;
    }
    const int batchIndex = batchCombo_->currentData().toInt();
    currentRecords_ = batchIndex < 0
                          ? allRecords_
                          : BuildAnalyzer::recordsForBatch(allRecords_, batches_.at(batchIndex));
    currentAnalysis_ = BuildAnalyzer::analyze(currentRecords_);
    refreshLoadedState();
}

void MainWindow::refreshLoadedState()
{
    if (!hasAnalysis_) {
        return;
    }

    int manifestMatches = 0;
    for (const NinjaLogRecord &record : allRecords_) {
        if (record.classificationSource == ClassificationSource::ManifestRule) {
            ++manifestMatches;
        }
    }
    diagnosticsLabel_->setText(
        tr("✓  已就绪  ·  %1  ·  %2 个有效任务  ·  %3 个构建批次  ·  %4")
            .arg(QFileInfo(currentLogPath_).fileName())
            .arg(allRecords_.size())
            .arg(batches_.size())
            .arg(manifest_.found()
                     ? tr("规则分类已匹配 %1/%2").arg(manifestMatches).arg(allRecords_.size())
                     : tr("按输出路径推断分类")));
    diagnosticsLabel_->show();
    analysisControls_->show();
    resultFrame_->show();
    initialLabel_->hide();
    refreshAnalysisViews();
}

void MainWindow::refreshAnalysisViews()
{
    const SummaryMetrics &summary = currentAnalysis_.summary;
    taskCountValue_->setText(QString::number(summary.taskCount));
    observedSpanValue_->setText(BuildAnalyzer::formatDuration(summary.observedSpanMs));
    totalTaskValue_->setText(BuildAnalyzer::formatDuration(summary.totalTaskMs));
    averageParallelValue_->setText(QString::number(summary.averageParallelism, 'f', 2));
    maximumParallelValue_->setText(QString::number(summary.maximumParallelism));

    if (!currentAnalysis_.categories.isEmpty() && !currentAnalysis_.slowest.isEmpty()) {
        const CategoryStats &topCategory = currentAnalysis_.categories.first();
        const NinjaLogRecord &slowest = currentAnalysis_.slowest.first();
        conclusionTitleLabel_->setText(
            tr("%1 内完成 %2 个任务，%3 是主要耗时来源")
                .arg(BuildAnalyzer::formatDuration(summary.observedSpanMs))
                .arg(summary.taskCount)
                .arg(categoryDisplayName(topCategory.category)));
        conclusionDetailLabel_->setText(
            tr("该类型占累计工作量 %1%；最慢单步是 %2，耗时 %3。")
                .arg(QString::number(topCategory.share * 100.0, 'f', 1))
                .arg(QFileInfo(slowest.output).fileName())
                .arg(BuildAnalyzer::formatDuration(slowest.durationMs())));
    } else {
        conclusionTitleLabel_->setText(tr("当前范围暂无可分析任务"));
        conclusionDetailLabel_->setText(tr("请切换构建批次或重新选择日志。"));
    }

    overviewCharts_->setAnalysis(currentAnalysis_);
    categoryTable_->setRowCount(currentAnalysis_.categories.size());
    for (int row = 0; row < currentAnalysis_.categories.size(); ++row) {
        const CategoryStats &stats = currentAnalysis_.categories.at(row);
        auto *categoryItem = textItem(categoryDisplayName(stats.category));
        categoryItem->setForeground(categoryColor(stats.category).darker(120));
        categoryItem->setData(Qt::UserRole, static_cast<int>(stats.category));
        if (row == 0) {
            QFont emphasizedFont = categoryItem->font();
            emphasizedFont.setBold(true);
            categoryItem->setFont(emphasizedFont);
        }
        categoryTable_->setItem(row, 0, categoryItem);
        categoryTable_->setItem(row, 1,
                                textItem(QString::number(stats.count), Qt::AlignRight));
        categoryTable_->setItem(row, 2,
                                textItem(BuildAnalyzer::formatDuration(stats.totalMs),
                                         Qt::AlignRight));
        categoryTable_->setItem(row, 3,
                                textItem(BuildAnalyzer::formatDuration(
                                             static_cast<qint64>(stats.averageMs + 0.5)),
                                         Qt::AlignRight));
        categoryTable_->setItem(row, 4,
                                textItem(BuildAnalyzer::formatDuration(stats.maximumMs),
                                         Qt::AlignRight));
        auto *shareBar = new QProgressBar(categoryTable_);
        shareBar->setRange(0, 1000);
        shareBar->setValue(static_cast<int>(stats.share * 1000.0 + 0.5));
        shareBar->setFormat(QStringLiteral("%1%").arg(
            QString::number(stats.share * 100.0, 'f', 1)));
        shareBar->setStyleSheet(QStringLiteral("QProgressBar::chunk { background: %1; }")
                                    .arg(categoryColor(stats.category).name()));
        categoryTable_->setCellWidget(row, 5, shareBar);
    }

    insightsList_->clear();
    for (const Insight &insight : currentAnalysis_.insights) {
        auto *item = new QListWidgetItem(
            QStringLiteral("●  %1\n%2").arg(insight.title, insight.detail), insightsList_);
        if (insight.severity == InsightSeverity::Warning) {
            item->setForeground(QColor(QStringLiteral("#B44F36")));
        } else if (insight.severity == InsightSeverity::Attention) {
            item->setForeground(QColor(QStringLiteral("#4E4EC7")));
        } else {
            item->setForeground(QColor(QStringLiteral("#596071")));
        }
        const int itemHeight = insight.detail.size() > 105 ? 94 : 72;
        item->setSizeHint(QSize(item->sizeHint().width(), itemHeight));
    }
    if (currentAnalysis_.insights.isEmpty()) {
        auto *item = new QListWidgetItem(tr("当前范围没有足够数据生成瓶颈提示。"), insightsList_);
        item->setForeground(QColor(QStringLiteral("#747B8C")));
    }

    applyFilters();
}

void MainWindow::applyFilters()
{
    if (!hasAnalysis_ || !slowTasksModel_) {
        return;
    }
    const int selectedCategory = categoryFilter_->currentData().toInt();
    const QString query = searchEdit_->text().trimmed();

    QVector<NinjaLogRecord> filtered;
    filtered.reserve(currentAnalysis_.slowest.size());
    for (const NinjaLogRecord &record : currentAnalysis_.slowest) {
        if (selectedCategory >= 0
            && static_cast<int>(record.category) != selectedCategory) {
            continue;
        }
        if (!query.isEmpty()
            && !record.output.contains(query, Qt::CaseInsensitive)) {
            continue;
        }
        filtered.append(record);
    }
    filteredRecords_ = std::move(filtered);
    slowTasksModel_->setRecords(filteredRecords_);
    timelineWidget_->setRecords(filteredRecords_);
    resultTabs_->setTabText(1, tr("慢任务  %1").arg(filteredRecords_.size()));
    resultTabs_->setTabText(2, tr("并发时间线  %1").arg(filteredRecords_.size()));
    filterStatusLabel_->setText(
        tr("按耗时从高到低排列  ·  显示 %1 / %2 个任务  ·  可用上方类型和路径继续筛选")
            .arg(filteredRecords_.size())
            .arg(currentAnalysis_.summary.taskCount));
    if (timelineWidget_->isTruncated()) {
        timelineStatusLabel_->setText(
            tr("为保持交互速度，时间线仅绘制当前过滤结果中耗时最长的 %1 / %2 条；统计和慢任务表仍为全量。")
                .arg(timelineWidget_->renderedRecordCount())
                .arg(timelineWidget_->totalRecordCount()));
    } else {
        timelineStatusLabel_->setText(
            tr("%1 个任务分布在 %2 条泳道  ·  横轴是相对构建时间  ·  悬停任务条查看详情")
                .arg(timelineWidget_->renderedRecordCount())
                .arg(timelineWidget_->laneCount()));
    }
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
        this,
        tr("选择 .ninja_log"),
        pathEdit_->text(),
        tr("Ninja 日志 (.ninja_log);;所有文件 (*)"));
    if (!selected.isEmpty()) {
        pathEdit_->setText(selected);
        analyzePath(selected);
    }
}

void MainWindow::selectDirectory()
{
    const QString selected = QFileDialog::getExistingDirectory(
        this,
        tr("选择构建目录"),
        pathEdit_->text());
    if (!selected.isEmpty()) {
        pathEdit_->setText(selected);
        analyzePath(selected);
    }
}

void MainWindow::showAbout()
{
    QMessageBox::information(
        this,
        tr("关于分析口径"),
        tr(".ninja_log 的开始/结束时间是 Ninja 进程内的相对毫秒。\n\n"
           "• 观察窗口：最早任务开始到最晚任务结束。\n"
           "• 累计任务时间：所有任务耗时之和，并行任务会重复计入。\n"
           "• 平均并行度：累计任务时间 ÷ 观察窗口。\n"
           "• 推断批次：日志行的结束时间发生回退时切分；日志被重整后可能不再对应真实构建。\n"
           "• 瓶颈提示：只描述日志可证明的耗时和重叠现象，不是 CPU/I/O 根因，也不是严格关键路径。\n\n"
           "本软件只读本地日志和 build.ninja，不运行构建、不修改文件、不上传数据。"));
}
