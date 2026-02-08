//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/sys_info/sys_info_widget_connections.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SysInfoWidgetConnections::SysInfoWidgetConnections(QWidget* parent)
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
            this, &SysInfoWidgetConnections::onContextMenu);

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
SysInfoWidgetConnections::~SysInfoWidgetConnections() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetConnections::category() const
{
    return common::kSystemInfo_Connections;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetConnections::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_connections())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::Connections& connections = system_info.connections();
    QIcon item_icon(":/img/connected.svg");

    for (int i = 0; i < connections.connection_size(); ++i)
    {
        const proto::system_info::Connections::Connection& connection = connections.connection(i);

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setIcon(0, item_icon);
        item->setText(0, QString::fromStdString(connection.process_name()));
        item->setText(1, QString::fromStdString(connection.protocol()));
        item->setText(2, QString::fromStdString(connection.local_address()));
        item->setText(3, QString::number(connection.local_port()));

        item->setText(4, QString::fromStdString(connection.remote_address()));
        if (connection.remote_port() != 0)
            item->setText(5, QString::number(connection.remote_port()));

        item->setText(6, QString::fromStdString(connection.state()));

        ui.tree->addTopLevelItem(item);
    }

    ui.tree->setColumnWidth(0, 150);
    ui.tree->setColumnWidth(1, 70);
    ui.tree->setColumnWidth(2, 110);
    ui.tree->setColumnWidth(3, 100);
    ui.tree->setColumnWidth(4, 110);
    ui.tree->setColumnWidth(5, 100);
    ui.tree->setColumnWidth(6, 100);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetConnections::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetConnections::onContextMenu(const QPoint& point)
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
