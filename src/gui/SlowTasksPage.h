#pragma once

#include "core/NinjaLogTypes.h"

#include <QWidget>

class QLabel;
class QTableView;
class SlowTasksModel;

class SlowTasksPage final : public QWidget {
    Q_OBJECT

public:
    explicit SlowTasksPage(QWidget *parent = nullptr);
    void setRecords(const QVector<ninja_analyzer::NinjaLogRecord> &records,
                    int totalTaskCount);

private:
    QLabel *statusLabel_ = nullptr;
    QTableView *view_ = nullptr;
    SlowTasksModel *model_ = nullptr;
};
