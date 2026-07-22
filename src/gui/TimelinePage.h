#pragma once

#include "core/NinjaLogTypes.h"

#include <QWidget>

class QLabel;
class TimelineWidget;

class TimelinePage final : public QWidget {
    Q_OBJECT

public:
    explicit TimelinePage(QWidget *parent = nullptr);
    void setRecords(const QVector<ninja_analyzer::NinjaLogRecord> &records);

private:
    QLabel *statusLabel_ = nullptr;
    TimelineWidget *timeline_ = nullptr;
};
