#include "gui/OverviewChartsWidget.h"

#include "core/BuildAnalyzer.h"
#include "gui/CategoryPalette.h"

#include <QFileInfo>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QToolTip>

using namespace ninja_analyzer;

namespace {

constexpr int kStackedBreakpoint = 620;
constexpr int kMaximumSlices = 5;
constexpr int kMaximumTaskBars = 5;

struct DisplaySlice {
    QString label;
    QColor color;
    qint64 totalMs = 0;
    double share = 0.0;
    int category = -1;
};

QPoint localMousePosition(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

QPoint globalMousePosition(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

QFont adjustedFont(const QFont &source, qreal pointDelta, bool bold = false)
{
    QFont font(source);
    font.setPointSizeF(qMax(8.0, source.pointSizeF() + pointDelta));
    font.setBold(bold);
    return font;
}

} // namespace

OverviewChartsWidget::OverviewChartsWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(280);
    setAccessibleName(tr("构建耗时图表"));
}

void OverviewChartsWidget::setAnalysis(const AnalysisResult &analysis)
{
    categories_ = analysis.categories;
    slowTasks_.clear();
    const int taskCount = qMin(kMaximumTaskBars, static_cast<int>(analysis.slowest.size()));
    slowTasks_.reserve(taskCount);
    for (int index = 0; index < taskCount; ++index) {
        slowTasks_.append(analysis.slowest.at(index));
    }
    hoveredRegion_ = -1;
    setAccessibleDescription(
        tr("%1 个步骤类型的耗时占比，以及耗时最长的 %2 个任务")
            .arg(categories_.size())
            .arg(slowTasks_.size()));
    updateGeometry();
    update();
}

QSize OverviewChartsWidget::sizeHint() const
{
    return QSize(900, 292);
}

int OverviewChartsWidget::heightForWidth(int width) const
{
    return width < kStackedBreakpoint ? 480 : 292;
}

void OverviewChartsWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    hitRegions_.clear();

    if (categories_.isEmpty() && slowTasks_.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#858C9B")));
        painter.drawText(rect(), Qt::AlignCenter, tr("当前范围没有可绘制的数据"));
        return;
    }

    const QRectF content = QRectF(rect()).adjusted(0, 2, 0, -2);
    if (width() < kStackedBreakpoint) {
        const qreal gap = 18.0;
        const qreal categoryHeight = 202.0;
        drawCategoryChart(painter, QRectF(content.left(), content.top(),
                                          content.width(), categoryHeight));
        painter.setPen(QColor(QStringLiteral("#E4E7ED")));
        painter.drawLine(QPointF(content.left(), content.top() + categoryHeight + gap / 2.0),
                         QPointF(content.right(), content.top() + categoryHeight + gap / 2.0));
        drawSlowTaskChart(
            painter,
            QRectF(content.left(), content.top() + categoryHeight + gap,
                   content.width(), content.height() - categoryHeight - gap));
    } else {
        const qreal gap = 26.0;
        const qreal categoryWidth = qRound((content.width() - gap) * 0.43);
        drawCategoryChart(painter, QRectF(content.left(), content.top(),
                                          categoryWidth, content.height()));
        painter.setPen(QColor(QStringLiteral("#E4E7ED")));
        const qreal dividerX = content.left() + categoryWidth + gap / 2.0;
        painter.drawLine(QPointF(dividerX, content.top() + 4),
                         QPointF(dividerX, content.bottom() - 4));
        drawSlowTaskChart(
            painter,
            QRectF(content.left() + categoryWidth + gap, content.top(),
                   content.width() - categoryWidth - gap, content.height()));
    }
}

void OverviewChartsWidget::drawCategoryChart(QPainter &painter, const QRectF &area)
{
    painter.setPen(QColor(QStringLiteral("#252A3A")));
    painter.setFont(adjustedFont(font(), 0.5, true));
    painter.drawText(QRectF(area.left(), area.top(), area.width(), 22),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("类型耗时占比"));
    painter.setPen(QColor(QStringLiteral("#8B92A1")));
    painter.setFont(adjustedFont(font(), -1.0));
    painter.drawText(QRectF(area.left(), area.top() + 23, area.width(), 18),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("累计任务时间 · 点击类型下钻"));

    QVector<DisplaySlice> slices;
    const int directSliceCount = qMin(kMaximumSlices, static_cast<int>(categories_.size()));
    slices.reserve(directSliceCount + 1);
    for (int index = 0; index < directSliceCount; ++index) {
        const CategoryStats &stats = categories_.at(index);
        slices.append(DisplaySlice{categoryDisplayName(stats.category),
                                   categoryColor(stats.category), stats.totalMs,
                                   stats.share, static_cast<int>(stats.category)});
    }
    if (categories_.size() > directSliceCount) {
        DisplaySlice other;
        other.label = tr("其他类型");
        other.color = QColor(QStringLiteral("#A8AFBD"));
        for (int index = directSliceCount; index < categories_.size(); ++index) {
            other.totalMs += categories_.at(index).totalMs;
            other.share += categories_.at(index).share;
        }
        slices.append(other);
    }

    const QRectF body = area.adjusted(0, 48, 0, 0);
    const qreal legendWidth = qBound(128.0, body.width() * 0.47, 190.0);
    const qreal chartWidth = qMax(88.0, body.width() - legendWidth - 12.0);
    const qreal diameter = qMin(qMin(chartWidth - 10.0, body.height() - 18.0), 142.0);
    const QPointF center(body.left() + chartWidth / 2.0,
                         body.top() + body.height() / 2.0);
    const QRectF outer(center.x() - diameter / 2.0, center.y() - diameter / 2.0,
                       diameter, diameter);
    const qreal innerDiameter = diameter * 0.58;
    const QRectF inner(center.x() - innerDiameter / 2.0,
                       center.y() - innerDiameter / 2.0,
                       innerDiameter, innerDiameter);

    qreal startAngle = 90.0;
    for (int index = 0; index < slices.size(); ++index) {
        const DisplaySlice &slice = slices.at(index);
        const qreal spanAngle = qMax(0.0, slice.share * 360.0);
        QPainterPath path;
        path.arcMoveTo(outer, startAngle);
        path.arcTo(outer, startAngle, -spanAngle);
        path.arcTo(inner, startAngle - spanAngle, spanAngle);
        path.closeSubpath();

        QColor fill = slice.color;
        const int regionIndex = hitRegions_.size();
        if (regionIndex == hoveredRegion_) {
            fill = fill.lighter(112);
        }
        painter.setPen(QPen(QColor(QStringLiteral("#FAFAFC")), 2.0));
        painter.setBrush(fill);
        painter.drawPath(path);

        if (slice.category >= 0) {
            hitRegions_.append(HitRegion{
                HitKind::Category,
                path,
                slice.category,
                {},
                tr("%1\n累计 %2 · 占比 %3%\n点击查看相关慢任务")
                    .arg(slice.label, BuildAnalyzer::formatDuration(slice.totalMs))
                    .arg(QString::number(slice.share * 100.0, 'f', 1))});
        }
        startAngle -= spanAngle;
    }

    if (!slices.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#252A3A")));
        painter.setFont(adjustedFont(font(), 1.5, true));
        painter.drawText(QRectF(center.x() - innerDiameter / 2.0,
                                center.y() - 15,
                                innerDiameter,
                                20),
                         Qt::AlignCenter,
                         QStringLiteral("%1%").arg(
                             QString::number(slices.first().share * 100.0, 'f', 1)));
        painter.setPen(QColor(QStringLiteral("#8B92A1")));
        painter.setFont(adjustedFont(font(), -1.5));
        painter.drawText(QRectF(center.x() - innerDiameter / 2.0,
                                center.y() + 4,
                                innerDiameter,
                                17),
                         Qt::AlignCenter, tr("最大占比"));
    }

    const qreal legendX = body.left() + chartWidth + 8.0;
    const qreal rowHeight = qMin(27.0, body.height() / qMax(1, slices.size()));
    const qreal legendTop = body.center().y() - rowHeight * slices.size() / 2.0;
    painter.setFont(adjustedFont(font(), -1.0));
    const QFontMetrics metrics(painter.font());
    for (int index = 0; index < slices.size(); ++index) {
        const DisplaySlice &slice = slices.at(index);
        const qreal rowY = legendTop + index * rowHeight;
        painter.setPen(Qt::NoPen);
        painter.setBrush(slice.color);
        painter.drawRoundedRect(QRectF(legendX, rowY + 6, 9, 9), 3, 3);

        painter.setPen(QColor(QStringLiteral("#4A5060")));
        const qreal percentWidth = 43.0;
        const QRectF labelRect(legendX + 15, rowY,
                               qMax(20.0, legendWidth - percentWidth - 19.0), rowHeight);
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(slice.label, Qt::ElideRight,
                                            static_cast<int>(labelRect.width())));
        painter.setPen(QColor(QStringLiteral("#6F7687")));
        painter.drawText(QRectF(body.right() - percentWidth, rowY,
                                percentWidth, rowHeight),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("%1%").arg(
                             QString::number(slice.share * 100.0, 'f', 1)));
    }
}

void OverviewChartsWidget::drawSlowTaskChart(QPainter &painter, const QRectF &area)
{
    painter.setPen(QColor(QStringLiteral("#252A3A")));
    painter.setFont(adjustedFont(font(), 0.5, true));
    painter.drawText(QRectF(area.left(), area.top(), area.width(), 22),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("最慢任务 Top 5"));
    painter.setPen(QColor(QStringLiteral("#8B92A1")));
    painter.setFont(adjustedFont(font(), -1.0));
    painter.drawText(QRectF(area.left(), area.top() + 23, area.width(), 18),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("统一时间尺度 · 点击任务定位详情"));

    if (slowTasks_.isEmpty()) {
        painter.drawText(area.adjusted(0, 46, 0, 0), Qt::AlignCenter,
                         tr("没有可显示的慢任务"));
        return;
    }

    const QRectF body = area.adjusted(0, 52, 0, -3);
    const qreal rowHeight = body.height() / slowTasks_.size();
    const qreal labelWidth = qBound(92.0, body.width() * 0.32, 176.0);
    const qreal valueWidth = 62.0;
    const qreal barLeft = body.left() + labelWidth + 12.0;
    const qreal barWidth = qMax(40.0, body.right() - barLeft - valueWidth - 8.0);
    const qint64 maximumDuration = qMax<qint64>(1, slowTasks_.first().durationMs());
    painter.setFont(adjustedFont(font(), -1.0));
    const QFontMetrics metrics(painter.font());

    for (int index = 0; index < slowTasks_.size(); ++index) {
        const NinjaLogRecord &record = slowTasks_.at(index);
        const qreal rowTop = body.top() + index * rowHeight;
        const QRectF rowRect(body.left(), rowTop, body.width(), rowHeight);
        const int regionIndex = hitRegions_.size();
        if (regionIndex == hoveredRegion_) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(QStringLiteral("#F0F0FF")));
            painter.drawRoundedRect(rowRect.adjusted(-3, 2, 3, -2), 6, 6);
        }

        const QString fileName = QFileInfo(record.output).fileName();
        painter.setPen(QColor(QStringLiteral("#4A5060")));
        painter.drawText(QRectF(body.left(), rowTop, labelWidth, rowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(fileName, Qt::ElideMiddle,
                                            static_cast<int>(labelWidth)));

        const qreal barHeight = qMin(15.0, rowHeight * 0.43);
        const qreal barY = rowTop + (rowHeight - barHeight) / 2.0;
        const QRectF track(barLeft, barY, barWidth, barHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#ECEEF3")));
        painter.drawRoundedRect(track, barHeight / 2.0, barHeight / 2.0);
        const qreal ratio = qBound(0.0,
                                   static_cast<double>(record.durationMs()) / maximumDuration,
                                   1.0);
        QRectF valueBar = track;
        valueBar.setWidth(qMax(5.0, track.width() * ratio));
        painter.setBrush(categoryColor(record.category));
        painter.drawRoundedRect(valueBar, barHeight / 2.0, barHeight / 2.0);

        painter.setPen(QColor(QStringLiteral("#343A49")));
        painter.drawText(QRectF(track.right() + 8.0, rowTop, valueWidth, rowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         BuildAnalyzer::formatDuration(record.durationMs()));

        QPainterPath hitPath;
        hitPath.addRect(rowRect);
        hitRegions_.append(HitRegion{
            HitKind::Task,
            hitPath,
            static_cast<int>(record.category),
            record.output,
            tr("%1\n%2 · %3\n点击在慢任务中定位")
                .arg(record.output,
                     categoryDisplayName(record.category),
                     BuildAnalyzer::formatDuration(record.durationMs()))});
    }
}

int OverviewChartsWidget::hitRegionAt(const QPoint &position) const
{
    for (int index = hitRegions_.size() - 1; index >= 0; --index) {
        if (hitRegions_.at(index).path.contains(position)) {
            return index;
        }
    }
    return -1;
}

void OverviewChartsWidget::mouseMoveEvent(QMouseEvent *event)
{
    const int hitIndex = hitRegionAt(localMousePosition(event));
    if (hitIndex != hoveredRegion_) {
        hoveredRegion_ = hitIndex;
        setCursor(hitIndex >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    if (hitIndex >= 0) {
        QToolTip::showText(globalMousePosition(event),
                           hitRegions_.at(hitIndex).tooltip, this);
    } else {
        QToolTip::hideText();
    }
}

void OverviewChartsWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const int hitIndex = hitRegionAt(localMousePosition(event));
    if (hitIndex < 0) {
        return;
    }
    const HitRegion &region = hitRegions_.at(hitIndex);
    if (region.kind == HitKind::Category) {
        emit categoryActivated(region.category);
    } else {
        emit taskActivated(region.output);
    }
}

void OverviewChartsWidget::leaveEvent(QEvent *event)
{
    hoveredRegion_ = -1;
    unsetCursor();
    QToolTip::hideText();
    update();
    QWidget::leaveEvent(event);
}
