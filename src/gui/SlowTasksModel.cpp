#include "gui/SlowTasksModel.h"

#include "core/BuildAnalyzer.h"
#include "gui/CategoryPalette.h"

#include <QBrush>

using namespace ninja_analyzer;

SlowTasksModel::SlowTasksModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int SlowTasksModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(records_.size());
}

int SlowTasksModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 7;
}

QVariant SlowTasksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= records_.size()) {
        return {};
    }
    const NinjaLogRecord &record = records_.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return record.output;
        case 1:
            return categoryDisplayName(record.category);
        case 2:
            return record.rule.isEmpty() ? QStringLiteral("—") : record.rule;
        case 3:
            return BuildAnalyzer::formatDuration(record.startMs);
        case 4:
            return BuildAnalyzer::formatDuration(record.endMs);
        case 5:
            return BuildAnalyzer::formatDuration(record.durationMs());
        case 6:
            return classificationSourceDisplayName(record.classificationSource);
        default:
            return {};
        }
    }
    if (role == Qt::ToolTipRole) {
        return QStringLiteral("输出：%1\n类型：%2\nRule：%3\n开始：%4 ms\n结束：%5 ms\n耗时：%6\n来源：%7\n日志行：%8")
            .arg(record.output,
                 categoryDisplayName(record.category),
                 record.rule.isEmpty() ? QStringLiteral("未匹配") : record.rule)
            .arg(record.startMs)
            .arg(record.endMs)
            .arg(BuildAnalyzer::formatDuration(record.durationMs()),
                 classificationSourceDisplayName(record.classificationSource))
            .arg(record.sourceLine);
    }
    if (role == Qt::ForegroundRole && index.column() == 1) {
        return QBrush(categoryColor(record.category).darker(115));
    }
    if (role == Qt::TextAlignmentRole && index.column() >= 3 && index.column() <= 5) {
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }
    if (role == Qt::UserRole) {
        switch (index.column()) {
        case 3:
            return record.startMs;
        case 4:
            return record.endMs;
        case 5:
            return record.durationMs();
        default:
            return {};
        }
    }
    return {};
}

QVariant SlowTasksModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    static const QStringList headers{
        QStringLiteral("输出"),
        QStringLiteral("步骤类型"),
        QStringLiteral("Rule"),
        QStringLiteral("开始"),
        QStringLiteral("结束"),
        QStringLiteral("耗时"),
        QStringLiteral("分类来源")};
    return section >= 0 && section < headers.size() ? headers.at(section) : QVariant{};
}

void SlowTasksModel::setRecords(const QVector<NinjaLogRecord> &records)
{
    beginResetModel();
    records_ = records;
    endResetModel();
}
