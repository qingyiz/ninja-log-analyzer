#include "gui/SlowTasksPage.h"

#include "gui/SlowTasksModel.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QTableView>
#include <QVBoxLayout>

SlowTasksPage::SlowTasksPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("filterStatus"));
    layout->addWidget(statusLabel_);
    model_ = new SlowTasksModel(this);
    view_ = new QTableView(this);
    view_->setObjectName(QStringLiteral("slowTasksView"));
    view_->setModel(model_);
    view_->setAlternatingRowColors(true);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setTextElideMode(Qt::ElideMiddle);
    view_->verticalHeader()->setDefaultSectionSize(30);
    view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    view_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(view_, 1);
}

void SlowTasksPage::setRecords(const QVector<ninja_analyzer::NinjaLogRecord> &records,
                               int totalTaskCount)
{
    model_->setRecords(records);
    statusLabel_->setText(
        tr("按耗时从高到低排列  ·  显示 %1 / %2 个任务  ·  可用上方类型和路径继续筛选")
            .arg(records.size())
            .arg(totalTaskCount));
}
