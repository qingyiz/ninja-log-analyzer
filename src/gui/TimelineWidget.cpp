#include "gui/TimelineWidget.h"

#include "core/BuildAnalyzer.h"
#include "gui/CategoryPalette.h"

#include <QFileInfo>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QToolTip>

#include <algorithm>

using namespace ninja_analyzer;

namespace {

constexpr int kHeaderHeight = 54;
constexpr int kLaneHeight = 22;
constexpr int kLaneGap = 5;
constexpr int kBottomMargin = 22;
constexpr int kLeftMargin = 68;
constexpr int kRightMargin = 24;

} // namespace

TimelineWidget::TimelineWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumWidth(900);
    setAutoFillBackground(true);
    setAccessibleName(tr("Ninja 任务并发时间线"));
}

void TimelineWidget::setRecords(const QVector<NinjaLogRecord> &records)
{
    layout_ = buildLayout(records);
    hitRegions_.clear();
    setAccessibleDescription(
        tr("显示 %1/%2 个任务，共 %3 条泳道")
            .arg(layout_.items.size())
            .arg(layout_.totalInputCount)
            .arg(layout_.laneCount));
    updateGeometry();
    update();
}

TimelineLayoutResult TimelineWidget::buildLayout(const QVector<NinjaLogRecord> &records,
                                                 int maximumRecords)
{
    TimelineLayoutResult result;
    result.totalInputCount = static_cast<int>(records.size());
    if (records.isEmpty() || maximumRecords <= 0) {
        result.truncated = !records.isEmpty();
        return result;
    }

    result.minStartMs = records.first().startMs;
    result.maxEndMs = records.first().endMs;
    for (const NinjaLogRecord &record : records) {
        result.minStartMs = qMin(result.minStartMs, record.startMs);
        result.maxEndMs = qMax(result.maxEndMs, record.endMs);
    }

    QVector<NinjaLogRecord> selected = records;
    if (selected.size() > maximumRecords) {
        std::sort(selected.begin(), selected.end(),
                  [](const NinjaLogRecord &left, const NinjaLogRecord &right) {
            if (left.durationMs() != right.durationMs()) {
                return left.durationMs() > right.durationMs();
            }
            const int outputOrder = QString::compare(left.output, right.output,
                                                     Qt::CaseSensitive);
            if (outputOrder != 0) {
                return outputOrder < 0;
            }
            return left.sourceLine < right.sourceLine;
        });
        selected.resize(maximumRecords);
        result.truncated = true;
    }
    std::sort(selected.begin(), selected.end(),
              [](const NinjaLogRecord &left, const NinjaLogRecord &right) {
        if (left.startMs != right.startMs) {
            return left.startMs < right.startMs;
        }
        if (left.endMs != right.endMs) {
            return left.endMs < right.endMs;
        }
        return left.output < right.output;
    });

    QVector<qint64> laneEnds;
    result.items.reserve(selected.size());
    for (const NinjaLogRecord &record : selected) {
        int selectedLane = -1;
        qint64 earliestReusableEnd = 0;
        for (int lane = 0; lane < laneEnds.size(); ++lane) {
            if (laneEnds.at(lane) <= record.startMs
                && (selectedLane < 0 || laneEnds.at(lane) < earliestReusableEnd)) {
                selectedLane = lane;
                earliestReusableEnd = laneEnds.at(lane);
            }
        }
        if (selectedLane < 0) {
            selectedLane = laneEnds.size();
            laneEnds.append(record.endMs);
        } else {
            laneEnds[selectedLane] = qMax(laneEnds.at(selectedLane), record.endMs);
        }
        result.items.append(TimelineLayoutItem{record, selectedLane});
    }
    result.laneCount = laneEnds.size();
    return result;
}

QSize TimelineWidget::sizeHint() const
{
    const int contentHeight = layout_.laneCount > 0
        ? kHeaderHeight + layout_.laneCount * (kLaneHeight + kLaneGap) + kBottomMargin
        : 280;
    return QSize(1200, qMax(280, contentHeight));
}

double TimelineWidget::xForTime(qint64 milliseconds, const QRectF &plotArea) const
{
    const qint64 span = layout_.maxEndMs - layout_.minStartMs;
    if (span <= 0) {
        return plotArea.left();
    }
    const double ratio = static_cast<double>(milliseconds - layout_.minStartMs) / span;
    return plotArea.left() + qBound(0.0, ratio, 1.0) * plotArea.width();
}

void TimelineWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(QStringLiteral("#FFFFFF")));
    hitRegions_.clear();

    if (layout_.items.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#7B8A9F")));
        painter.drawText(rect(), Qt::AlignCenter,
                         tr("当前过滤条件下没有可绘制的任务。"));
        return;
    }

    const QRectF plotArea(kLeftMargin,
                          kHeaderHeight,
                          qMax(1, width() - kLeftMargin - kRightMargin),
                          qMax(1, height() - kHeaderHeight - kBottomMargin));
    painter.setPen(QColor(QStringLiteral("#344054")));
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRectF(kLeftMargin, 10, width() - kLeftMargin - kRightMargin, 20),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("相对时间轴 · %1 条泳道 · 绘制 %2/%3 个任务")
                         .arg(layout_.laneCount)
                         .arg(layout_.items.size())
                         .arg(layout_.totalInputCount));

    QFont axisFont = painter.font();
    axisFont.setBold(false);
    axisFont.setPointSizeF(qMax(8.0, axisFont.pointSizeF() - 1.0));
    painter.setFont(axisFont);
    for (int tick = 0; tick <= 10; ++tick) {
        const double ratio = static_cast<double>(tick) / 10.0;
        const double x = plotArea.left() + plotArea.width() * ratio;
        painter.setPen(QColor(QStringLiteral("#E5EAF1")));
        painter.drawLine(QPointF(x, kHeaderHeight - 4), QPointF(x, height() - kBottomMargin));
        const qint64 relativeTime = static_cast<qint64>(
            (layout_.maxEndMs - layout_.minStartMs) * ratio);
        painter.setPen(QColor(QStringLiteral("#667085")));
        const QRectF labelRect(x - 45, 31, 90, 18);
        painter.drawText(labelRect, Qt::AlignHCenter | Qt::AlignVCenter,
                         BuildAnalyzer::formatDuration(relativeTime));
    }

    for (int lane = 0; lane < layout_.laneCount; ++lane) {
        const double y = kHeaderHeight + lane * (kLaneHeight + kLaneGap);
        if (lane % 2 == 1) {
            painter.fillRect(QRectF(0, y - 2, width(), kLaneHeight + 4),
                             QColor(QStringLiteral("#FAFBFD")));
        }
        painter.setPen(QColor(QStringLiteral("#98A2B3")));
        painter.drawText(QRectF(8, y, kLeftMargin - 16, kLaneHeight),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("L%1").arg(lane + 1));
    }

    const QFontMetrics metrics(painter.font());
    for (int index = 0; index < layout_.items.size(); ++index) {
        const TimelineLayoutItem &item = layout_.items.at(index);
        const double x1 = xForTime(item.record.startMs, plotArea);
        const double x2 = xForTime(item.record.endMs, plotArea);
        const double y = kHeaderHeight + item.lane * (kLaneHeight + kLaneGap);
        QRectF bar(x1, y, qMax(3.0, x2 - x1), kLaneHeight);
        if (!bar.intersects(event->rect())) {
            continue;
        }

        QColor fill = categoryColor(item.record.category);
        painter.setPen(fill.darker(118));
        painter.setBrush(fill);
        painter.drawRoundedRect(bar, 4, 4);
        hitRegions_.append(HitRegion{bar, index});

        if (bar.width() >= 72) {
            const QString label = QStringLiteral("%1  %2")
                .arg(QFileInfo(item.record.output).fileName(),
                     BuildAnalyzer::formatDuration(item.record.durationMs()));
            painter.setPen(Qt::white);
            painter.drawText(bar.adjusted(6, 0, -5, 0),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             metrics.elidedText(label, Qt::ElideRight,
                                                static_cast<int>(bar.width() - 11)));
        }
    }
}

QString TimelineWidget::tooltipForItem(const TimelineLayoutItem &item) const
{
    return tr("%1\n类型：%2\nRule：%3\n开始：%4 ms  ·  结束：%5 ms\n耗时：%6\n分类：%7")
        .arg(item.record.output,
             categoryDisplayName(item.record.category),
             item.record.rule.isEmpty() ? tr("未匹配") : item.record.rule)
        .arg(item.record.startMs)
        .arg(item.record.endMs)
        .arg(BuildAnalyzer::formatDuration(item.record.durationMs()),
             classificationSourceDisplayName(item.record.classificationSource));
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint localPosition = event->position().toPoint();
    const QPoint globalPosition = event->globalPosition().toPoint();
#else
    const QPoint localPosition = event->pos();
    const QPoint globalPosition = event->globalPos();
#endif
    for (auto iterator = hitRegions_.crbegin(); iterator != hitRegions_.crend(); ++iterator) {
        if (iterator->rectangle.contains(localPosition)) {
            QToolTip::showText(globalPosition,
                               tooltipForItem(layout_.items.at(iterator->itemIndex)),
                               this,
                               iterator->rectangle.toRect());
            return;
        }
    }
    QToolTip::hideText();
}

void TimelineWidget::leaveEvent(QEvent *event)
{
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}
