//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/ui/sys_info/sys_info_widget_open_files.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SysInfoWidgetOpenFiles::SysInfoWidgetOpenFiles(QWidget* parent)
    : SysInfoWidget(parent)
{
    ui.setupUi(this);
    ui.tree->setMouseTracking(true);

    connect(ui.action_copy_row, &QAction::triggered, this, [this]()
    {
        copyRow(ui.tree->currentItem());
    });

    connect(ui.action_copy_value, &QAction::triggered, this, [this]()
    {
        copyColumn(ui.tree->currentItem(), current_column_);
    });

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetOpenFiles::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });

    connect(ui.tree, &QTreeWidget::itemEntered,
            this, [this](QTreeWidgetItem* /* item */, int column)
    {
        current_column_ = column;
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetOpenFiles::~SysInfoWidgetOpenFiles() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetOpenFiles::category() const
{
    return common::kSystemInfo_OpenFiles;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetOpenFiles::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_open_files())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::OpenFiles& open_files = system_info.open_files();
    QIcon item_icon(":/img/nas.svg");

    for (int i = 0; i < open_files.open_file_size(); ++i)
    {
        const proto::system_info::OpenFiles::OpenFile& open_file =
            open_files.open_file(i);

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setIcon(0, item_icon);
        item->setText(0, QString::fromStdString(open_file.file_path()));
        item->setText(1, QString::fromStdString(open_file.user_name()));
        item->setText(2, QString::number(open_file.lock_count()));

        ui.tree->addTopLevelItem(item);
    }

    ui.tree->setColumnWidth(0, 250);
    ui.tree->setColumnWidth(1, 120);
    ui.tree->setColumnWidth(2, 100);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetOpenFiles::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetOpenFiles::onContextMenu(const QPoint& point)
{
    QTreeWidgetItem* current_item = ui.tree->itemAt(point);
    if (!current_item)
        return;

    ui.tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui.action_copy_row);
    menu.addAction(ui.action_copy_value);

    menu.exec(ui.tree->viewport()->mapToGlobal(point));
}

} // namespace client
