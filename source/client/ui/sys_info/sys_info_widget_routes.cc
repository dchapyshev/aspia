//
// Aspia Project
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

#include "client/ui/sys_info/sys_info_widget_routes.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SysInfoWidgetRoutes::SysInfoWidgetRoutes(QWidget* parent)
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
            this, &SysInfoWidgetRoutes::onContextMenu);

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
SysInfoWidgetRoutes::~SysInfoWidgetRoutes() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetRoutes::category() const
{
    return common::kSystemInfo_Routes;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetRoutes::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_routes())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::Routes& routes = system_info.routes();
    QIcon item_icon(":/img/flow-chart.svg");

    for (int i = 0; i < routes.route_size(); ++i)
    {
        const proto::system_info::Routes::Route& route = routes.route(i);

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setIcon(0, item_icon);
        item->setText(0, QString::fromStdString(route.destonation()));
        item->setText(1, QString::fromStdString(route.mask()));
        item->setText(2, QString::fromStdString(route.gateway()));
        item->setText(3, QString::number(route.metric()));

        ui.tree->addTopLevelItem(item);
    }

    ui.tree->setColumnWidth(0, 150);
    ui.tree->setColumnWidth(1, 150);
    ui.tree->setColumnWidth(2, 150);
    ui.tree->setColumnWidth(3, 100);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetRoutes::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetRoutes::onContextMenu(const QPoint& point)
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
