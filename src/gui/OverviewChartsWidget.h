#pragma once

#include "core/NinjaLogTypes.h"

#include <QPainterPath>
#include <QWidget>

class QPainter;

class OverviewChartsWidget final : public QWidget {
    Q_OBJECT

public:
    explicit OverviewChartsWidget(QWidget *parent = nullptr);

    void setAnalysis(const ninja_analyzer::AnalysisResult &analysis);
    int categoryCount() const { return categories_.size(); }
    int taskBarCount() const { return slowTasks_.size(); }

    QSize sizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

signals:
    void categoryActivated(int category);
    void taskActivated(const QString &output);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class HitKind {
        Category,
        Task
    };

    struct HitRegion {
        HitKind kind = HitKind::Category;
        QPainterPath path;
        int category = -1;
        QString output;
        QString tooltip;
    };

    void drawCategoryChart(QPainter &painter, const QRectF &area);
    void drawSlowTaskChart(QPainter &painter, const QRectF &area);
    int hitRegionAt(const QPoint &position) const;

    QVector<ninja_analyzer::CategoryStats> categories_;
    QVector<ninja_analyzer::NinjaLogRecord> slowTasks_;
    QVector<HitRegion> hitRegions_;
    int hoveredRegion_ = -1;
};
