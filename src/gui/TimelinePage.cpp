#include "gui/TimelinePage.h"

#include "gui/TimelineWidget.h"

#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

TimelinePage::TimelinePage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("timelineStatus"));
    layout->addWidget(statusLabel_);
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("timelineScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    timeline_ = new TimelineWidget(scroll);
    timeline_->setObjectName(QStringLiteral("timelineWidget"));
    scroll->setWidget(timeline_);
    layout->addWidget(scroll, 1);
}

void TimelinePage::setRecords(const QVector<ninja_analyzer::NinjaLogRecord> &records)
{
    timeline_->setRecords(records);
    if (timeline_->isTruncated()) {
        statusLabel_->setText(
            tr("为保持交互速度，时间线仅绘制当前过滤结果中耗时最长的 %1 / %2 条；统计和慢任务表仍为全量。")
                .arg(timeline_->renderedRecordCount())
                .arg(timeline_->totalRecordCount()));
    } else {
        statusLabel_->setText(
            tr("%1 个任务分布在 %2 条泳道  ·  横轴是相对构建时间  ·  悬停任务条查看详情")
                .arg(timeline_->renderedRecordCount())
                .arg(timeline_->laneCount()));
    }
}
