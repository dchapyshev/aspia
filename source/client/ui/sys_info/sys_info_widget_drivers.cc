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

#include "client/ui/sys_info/sys_info_widget_drivers.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SysInfoWidgetDrivers::SysInfoWidgetDrivers(QWidget* parent)
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
            this, &SysInfoWidgetDrivers::onContextMenu);

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
SysInfoWidgetDrivers::~SysInfoWidgetDrivers() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetDrivers::category() const
{
    return common::kSystemInfo_Drivers;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrivers::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_drivers())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::Drivers& drivers = system_info.drivers();
    QIcon item_icon(":/img/network-card.svg");

    for (int i = 0; i < drivers.driver_size(); ++i)
    {
        const proto::system_info::Drivers::Driver& driver = drivers.driver(i);

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setIcon(0, item_icon);
        item->setText(0, QString::fromStdString(driver.display_name()));
        item->setText(1, QString::fromStdString(driver.name()));
        item->setText(2, QString::fromStdString(driver.description()));
        item->setText(3, statusToString(driver.status()));
        item->setText(4, startupTypeToString(driver.startup_type()));
        item->setText(5, QString::fromStdString(driver.binary_path()));

        ui.tree->addTopLevelItem(item);
    }

    ui.tree->setColumnWidth(0, 200);
    ui.tree->setColumnWidth(1, 100);
    ui.tree->setColumnWidth(2, 200);
    ui.tree->setColumnWidth(3, 70);
    ui.tree->setColumnWidth(4, 100);
    ui.tree->setColumnWidth(5, 200);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetDrivers::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrivers::onContextMenu(const QPoint& point)
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

//--------------------------------------------------------------------------------------------------
// static
QString SysInfoWidgetDrivers::statusToString(proto::system_info::Drivers::Driver::Status status)
{
    switch (status)
    {
        case proto::system_info::Drivers::Driver::STATUS_CONTINUE_PENDING:
            return tr("Continue Pending");

        case proto::system_info::Drivers::Driver::STATUS_PAUSE_PENDING:
            return tr("Pause Pending");

        case proto::system_info::Drivers::Driver::STATUS_PAUSED:
            return tr("Paused");

        case proto::system_info::Drivers::Driver::STATUS_RUNNING:
            return tr("Running");

        case proto::system_info::Drivers::Driver::STATUS_START_PENDING:
            return tr("Start Pending");

        case proto::system_info::Drivers::Driver::STATUS_STOP_PENDING:
            return tr("Stop Pending");

        case proto::system_info::Drivers::Driver::STATUS_STOPPED:
            return tr("Stopped");

        default:
            return tr("Unknown");
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfoWidgetDrivers::startupTypeToString(
    proto::system_info::Drivers::Driver::StartupType startup_type)
{
    switch (startup_type)
    {
        case proto::system_info::Drivers::Driver::STARTUP_TYPE_AUTO_START:
            return tr("Auto Start");

        case proto::system_info::Drivers::Driver::STARTUP_TYPE_DEMAND_START:
            return tr("Demand Start");

        case proto::system_info::Drivers::Driver::STARTUP_TYPE_DISABLED:
            return tr("Disabled");

        case proto::system_info::Drivers::Driver::STARTUP_TYPE_BOOT_START:
            return tr("Boot Start");

        case proto::system_info::Drivers::Driver::STARTUP_TYPE_SYSTEM_START:
            return tr("System Start");

        default:
            return tr("Unknown");
    }
}

} // namespace client
