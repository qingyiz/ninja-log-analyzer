#pragma once

#include "core/NinjaLogTypes.h"

#include <QRectF>
#include <QWidget>

struct TimelineLayoutItem {
    ninja_analyzer::NinjaLogRecord record;
    int lane = 0;
};

struct TimelineLayoutResult {
    QVector<TimelineLayoutItem> items;
    int laneCount = 0;
    int totalInputCount = 0;
    qint64 minStartMs = 0;
    qint64 maxEndMs = 0;
    bool truncated = false;
};

class TimelineWidget final : public QWidget {
public:
    static constexpr int MaximumRenderedRecords = 5000;

    explicit TimelineWidget(QWidget *parent = nullptr);

    void setRecords(const QVector<ninja_analyzer::NinjaLogRecord> &records);
    int renderedRecordCount() const { return layout_.items.size(); }
    int totalRecordCount() const { return layout_.totalInputCount; }
    int laneCount() const { return layout_.laneCount; }
    bool isTruncated() const { return layout_.truncated; }
    const TimelineLayoutResult &layoutResult() const { return layout_; }

    static TimelineLayoutResult buildLayout(
        const QVector<ninja_analyzer::NinjaLogRecord> &records,
        int maximumRecords = MaximumRenderedRecords);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct HitRegion {
        QRectF rectangle;
        int itemIndex = -1;
    };

    double xForTime(qint64 milliseconds, const QRectF &plotArea) const;
    QString tooltipForItem(const TimelineLayoutItem &item) const;

    TimelineLayoutResult layout_;
    QVector<HitRegion> hitRegions_;
};
