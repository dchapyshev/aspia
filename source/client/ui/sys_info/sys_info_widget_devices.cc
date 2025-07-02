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

#include "client/ui/sys_info/sys_info_widget_devices.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SysInfoWidgetDevices::SysInfoWidgetDevices(QWidget* parent)
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
            this, &SysInfoWidgetDevices::onContextMenu);

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

    connect(ui.action_find_device, &QAction::triggered, this, [this]()
    {
        QTreeWidgetItem* item = ui.tree->currentItem();
        if (!item)
            return;

        QString device_id = item->text(4);
        if (device_id.isEmpty())
            return;

        searchInGoogle(device_id);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetDevices::~SysInfoWidgetDevices() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetDevices::category() const
{
    return common::kSystemInfo_Devices;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDevices::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_windows_devices())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::WindowsDevices& devices = system_info.windows_devices();
    QIcon item_icon(":/img/network-card.svg");
    QTreeWidget* tree = ui.tree;

    for (int i = 0; i < devices.device_size(); ++i)
    {
        const proto::system_info::WindowsDevices::Device& device = devices.device(i);

        QString name = QString::fromStdString(device.friendly_name());
        if (name.isEmpty())
            name = QString::fromStdString(device.description());

        if (name.isEmpty())
            continue;

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setIcon(0, item_icon);
        item->setText(0, name);
        item->setText(1, QString::fromStdString(device.driver_version()));
        item->setText(2, QString::fromStdString(device.driver_date()));
        item->setText(3, QString::fromStdString(device.driver_vendor()));
        item->setText(4, QString::fromStdString(device.device_id()));

        tree->addTopLevelItem(item);
    }

    ui.tree->setColumnWidth(0, 200);
    ui.tree->setColumnWidth(1, 90);
    ui.tree->setColumnWidth(2, 90);
    ui.tree->setColumnWidth(3, 90);
    ui.tree->setColumnWidth(4, 200);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetDevices::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDevices::onContextMenu(const QPoint& point)
{
    QTreeWidgetItem* current_item = ui.tree->itemAt(point);
    if (!current_item)
        return;

    ui.tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui.action_copy_row);
    menu.addAction(ui.action_copy_value);
    menu.addSeparator();
    menu.addAction(ui.action_find_device);

    menu.exec(ui.tree->viewport()->mapToGlobal(point));
}

} // namespace client
