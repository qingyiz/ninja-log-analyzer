#pragma once

#include "core/NinjaLogTypes.h"

#include <QAbstractTableModel>

class SlowTasksModel final : public QAbstractTableModel {
public:
    explicit SlowTasksModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void setRecords(const QVector<ninja_analyzer::NinjaLogRecord> &records);
    const QVector<ninja_analyzer::NinjaLogRecord> &records() const { return records_; }

private:
    QVector<ninja_analyzer::NinjaLogRecord> records_;
};
