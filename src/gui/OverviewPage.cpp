#include "gui/OverviewPage.h"

#include "core/BuildAnalyzer.h"
#include "gui/CategoryPalette.h"
#include "gui/OverviewChartsWidget.h"

#include <QAbstractItemView>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

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
    value->setObjectName(valueObjectName);
    value->setAccessibleName(valueObjectName);
    auto *hintLabel = new QLabel(hint, card);
    hintLabel->setObjectName(QStringLiteral("metricHint"));
    layout->addWidget(captionLabel);
    layout->addWidget(value);
    layout->addWidget(hintLabel);
    *valueLabel = value;
    return card;
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

OverviewPage::OverviewPage(QWidget *parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *conclusionPanel = new QFrame(content);
    conclusionPanel->setObjectName(QStringLiteral("conclusionPanel"));
    auto *conclusionLayout = new QHBoxLayout(conclusionPanel);
    conclusionLayout->setContentsMargins(20, 16, 14, 16);
    conclusionLayout->setSpacing(18);
    auto *copy = new QVBoxLayout;
    auto *eyebrow = new QLabel(tr("分析结论"), conclusionPanel);
    eyebrow->setObjectName(QStringLiteral("conclusionEyebrow"));
    conclusionTitleLabel_ = new QLabel(conclusionPanel);
    conclusionTitleLabel_->setObjectName(QStringLiteral("conclusionTitle"));
    conclusionTitleLabel_->setWordWrap(true);
    conclusionDetailLabel_ = new QLabel(conclusionPanel);
    conclusionDetailLabel_->setObjectName(QStringLiteral("conclusionDetail"));
    conclusionDetailLabel_->setWordWrap(true);
    copy->addWidget(eyebrow);
    copy->addWidget(conclusionTitleLabel_);
    copy->addWidget(conclusionDetailLabel_);
    conclusionLayout->addLayout(copy, 1);
    auto *slowButton = new QPushButton(tr("查看最慢任务"), conclusionPanel);
    slowButton->setObjectName(QStringLiteral("inlineButton"));
    connect(slowButton, &QPushButton::clicked, this, &OverviewPage::showSlowTasksRequested);
    conclusionLayout->addWidget(slowButton, 0, Qt::AlignVCenter);
    auto *timelineButton = new QPushButton(tr("打开时间线  →"), conclusionPanel);
    timelineButton->setObjectName(QStringLiteral("inlinePrimaryButton"));
    connect(timelineButton, &QPushButton::clicked, this, &OverviewPage::showTimelineRequested);
    conclusionLayout->addWidget(timelineButton, 0, Qt::AlignVCenter);
    layout->addWidget(conclusionPanel);

    auto *metrics = new QHBoxLayout;
    metrics->setSpacing(10);
    metrics->addWidget(createMetricCard(content, tr("构建耗时"), QStringLiteral("summarySpanValue"),
                                        tr("从首个任务到最后结束"), QStringLiteral("primary"),
                                        &observedSpanValue_), 2);
    metrics->addWidget(createMetricCard(content, tr("任务数量"), QStringLiteral("summaryTaskValue"),
                                        tr("当前批次内的构建步骤"), QStringLiteral("neutral"),
                                        &taskCountValue_));
    metrics->addWidget(createMetricCard(content, tr("累计工作量"), QStringLiteral("summaryTotalValue"),
                                        tr("并行任务会重复计入"), QStringLiteral("neutral"),
                                        &totalTaskValue_));
    metrics->addWidget(createMetricCard(content, tr("平均并行"), QStringLiteral("summaryAverageValue"),
                                        tr("任务重叠的平均水平"), QStringLiteral("success"),
                                        &averageParallelValue_));
    metrics->addWidget(createMetricCard(content, tr("峰值并行"), QStringLiteral("summaryMaximumValue"),
                                        tr("同时运行任务的峰值"), QStringLiteral("success"),
                                        &maximumParallelValue_));
    layout->addLayout(metrics);

    auto *splitter = new QSplitter(Qt::Horizontal, content);
    auto *categoryPanel = new QFrame(splitter);
    categoryPanel->setObjectName(QStringLiteral("innerPanel"));
    auto *categoryLayout = new QVBoxLayout(categoryPanel);
    auto *categoryHeader = new QHBoxLayout;
    auto *categoryTitle = new QLabel(tr("耗时都花在哪里"), categoryPanel);
    categoryTitle->setObjectName(QStringLiteral("panelTitle"));
    categoryHeader->addWidget(categoryTitle);
    categoryHeader->addStretch();
    auto *toggle = new QPushButton(tr("查看数据表"), categoryPanel);
    toggle->setObjectName(QStringLiteral("compactButton"));
    toggle->setCheckable(true);
    categoryHeader->addWidget(toggle);
    categoryLayout->addLayout(categoryHeader);
    auto *categorySubtitle = new QLabel(tr("按累计任务时间排序；点击任一类型查看相关慢任务"), categoryPanel);
    categorySubtitle->setObjectName(QStringLiteral("panelSubtitle"));
    categoryLayout->addWidget(categorySubtitle);
    overviewCharts_ = new OverviewChartsWidget(categoryPanel);
    overviewCharts_->setObjectName(QStringLiteral("overviewCharts"));
    connect(overviewCharts_, &OverviewChartsWidget::categoryActivated,
            this, &OverviewPage::categoryActivated);
    connect(overviewCharts_, &OverviewChartsWidget::taskActivated,
            this, &OverviewPage::taskActivated);
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
    categoryTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 6; ++column) {
        categoryTable_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    connect(categoryTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QTableWidgetItem *item = categoryTable_->item(row, 0);
        if (item) {
            emit categoryActivated(item->data(Qt::UserRole).toInt());
        }
    });
    categoryLayout->addWidget(categoryTable_);
    categoryTable_->hide();
    connect(toggle, &QPushButton::toggled, this, [this, toggle](bool checked) {
        categoryTable_->setVisible(checked);
        toggle->setText(checked ? tr("隐藏数据表") : tr("查看数据表"));
    });

    auto *insightsPanel = new QFrame(splitter);
    insightsPanel->setObjectName(QStringLiteral("innerPanel"));
    auto *insightsLayout = new QVBoxLayout(insightsPanel);
    auto *insightsTitle = new QLabel(tr("建议先看这些"), insightsPanel);
    insightsTitle->setObjectName(QStringLiteral("panelTitle"));
    insightsLayout->addWidget(insightsTitle);
    auto *insightsSubtitle = new QLabel(tr("根据耗时、并行和结束位置自动生成"), insightsPanel);
    insightsSubtitle->setObjectName(QStringLiteral("panelSubtitle"));
    insightsLayout->addWidget(insightsSubtitle);
    insightsList_ = new QListWidget(insightsPanel);
    insightsList_->setObjectName(QStringLiteral("insightsList"));
    insightsList_->setWordWrap(true);
    insightsList_->setSelectionMode(QAbstractItemView::NoSelection);
    insightsLayout->addWidget(insightsList_);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);
    setWidget(content);
}

void OverviewPage::setAnalysis(const AnalysisResult &analysis)
{
    const SummaryMetrics &summary = analysis.summary;
    taskCountValue_->setText(QString::number(summary.taskCount));
    observedSpanValue_->setText(BuildAnalyzer::formatDuration(summary.observedSpanMs));
    totalTaskValue_->setText(BuildAnalyzer::formatDuration(summary.totalTaskMs));
    averageParallelValue_->setText(QString::number(summary.averageParallelism, 'f', 2));
    maximumParallelValue_->setText(QString::number(summary.maximumParallelism));

    if (!analysis.categories.isEmpty() && !analysis.slowest.isEmpty()) {
        const CategoryStats &top = analysis.categories.first();
        const NinjaLogRecord &slowest = analysis.slowest.first();
        conclusionTitleLabel_->setText(
            tr("%1 内完成 %2 个任务，%3 是主要耗时来源")
                .arg(BuildAnalyzer::formatDuration(summary.observedSpanMs))
                .arg(summary.taskCount)
                .arg(categoryDisplayName(top.category)));
        conclusionDetailLabel_->setText(
            tr("该类型占累计工作量 %1%；最慢单步是 %2，耗时 %3。")
                .arg(QString::number(top.share * 100.0, 'f', 1))
                .arg(QFileInfo(slowest.output).fileName())
                .arg(BuildAnalyzer::formatDuration(slowest.durationMs())));
    } else {
        conclusionTitleLabel_->setText(tr("当前范围暂无可分析任务"));
        conclusionDetailLabel_->setText(tr("请切换构建批次或重新选择日志。"));
    }

    overviewCharts_->setAnalysis(analysis);
    categoryTable_->setRowCount(analysis.categories.size());
    for (int row = 0; row < analysis.categories.size(); ++row) {
        const CategoryStats &stats = analysis.categories.at(row);
        auto *categoryItem = textItem(categoryDisplayName(stats.category));
        categoryItem->setForeground(categoryColor(stats.category).darker(120));
        categoryItem->setData(Qt::UserRole, static_cast<int>(stats.category));
        categoryTable_->setItem(row, 0, categoryItem);
        categoryTable_->setItem(row, 1, textItem(QString::number(stats.count), Qt::AlignRight));
        categoryTable_->setItem(row, 2, textItem(BuildAnalyzer::formatDuration(stats.totalMs), Qt::AlignRight));
        categoryTable_->setItem(row, 3, textItem(BuildAnalyzer::formatDuration(
            static_cast<qint64>(stats.averageMs + 0.5)), Qt::AlignRight));
        categoryTable_->setItem(row, 4, textItem(BuildAnalyzer::formatDuration(stats.maximumMs), Qt::AlignRight));
        auto *share = new QProgressBar(categoryTable_);
        share->setRange(0, 1000);
        share->setValue(static_cast<int>(stats.share * 1000.0 + 0.5));
        share->setFormat(QStringLiteral("%1%").arg(QString::number(stats.share * 100.0, 'f', 1)));
        share->setStyleSheet(QStringLiteral("QProgressBar::chunk { background: %1; }")
                                 .arg(categoryColor(stats.category).name()));
        categoryTable_->setCellWidget(row, 5, share);
    }

    insightsList_->clear();
    for (const Insight &insight : analysis.insights) {
        auto *item = new QListWidgetItem(
            QStringLiteral("●  %1\n%2").arg(insight.title, insight.detail), insightsList_);
        item->setForeground(insight.severity == InsightSeverity::Warning
                                ? QColor(QStringLiteral("#B44F36"))
                            : insight.severity == InsightSeverity::Attention
                                ? QColor(QStringLiteral("#4E4EC7"))
                                : QColor(QStringLiteral("#596071")));
        item->setSizeHint(QSize(item->sizeHint().width(), insight.detail.size() > 105 ? 94 : 72));
    }
    if (analysis.insights.isEmpty()) {
        insightsList_->addItem(tr("当前范围没有足够数据生成瓶颈提示。"));
    }
}
