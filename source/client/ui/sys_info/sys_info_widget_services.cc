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

#include "client/ui/sys_info/sys_info_widget_services.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SysInfoWidgetServices::SysInfoWidgetServices(QWidget* parent)
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
            this, &SysInfoWidgetServices::onContextMenu);

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
SysInfoWidgetServices::~SysInfoWidgetServices() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetServices::category() const
{
    return common::kSystemInfo_Services;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetServices::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_services())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::Services& services = system_info.services();
    QIcon item_icon(":/img/gear.svg");

    for (int i = 0; i < services.service_size(); ++i)
    {
        const proto::system_info::Services::Service& service = services.service(i);

        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setIcon(0, item_icon);
        item->setText(0, QString::fromStdString(service.display_name()));
        item->setText(1, QString::fromStdString(service.name()));
        item->setText(2, QString::fromStdString(service.description()));
        item->setText(3, statusToString(service.status()));
        item->setText(4, startupTypeToString(service.startup_type()));
        item->setText(5, QString::fromStdString(service.start_name()));
        item->setText(6, QString::fromStdString(service.binary_path()));

        ui.tree->addTopLevelItem(item);
    }

    ui.tree->setColumnWidth(0, 200);
    ui.tree->setColumnWidth(1, 150);
    ui.tree->setColumnWidth(2, 200);
    ui.tree->setColumnWidth(3, 70);
    ui.tree->setColumnWidth(4, 100);
    ui.tree->setColumnWidth(5, 150);
    ui.tree->setColumnWidth(6, 200);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetServices::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetServices::onContextMenu(const QPoint& point)
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
QString SysInfoWidgetServices::statusToString(proto::system_info::Services::Service::Status status)
{
    switch (status)
    {
        case proto::system_info::Services::Service::STATUS_CONTINUE_PENDING:
            return tr("Continue Pending");

        case proto::system_info::Services::Service::STATUS_PAUSE_PENDING:
            return tr("Pause Pending");

        case proto::system_info::Services::Service::STATUS_PAUSED:
            return tr("Paused");

        case proto::system_info::Services::Service::STATUS_RUNNING:
            return tr("Running");

        case proto::system_info::Services::Service::STATUS_START_PENDING:
            return tr("Start Pending");

        case proto::system_info::Services::Service::STATUS_STOP_PENDING:
            return tr("Stop Pending");

        case proto::system_info::Services::Service::STATUS_STOPPED:
            return tr("Stopped");

        default:
            return tr("Unknown");
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfoWidgetServices::startupTypeToString(
    proto::system_info::Services::Service::StartupType startup_type)
{
    switch (startup_type)
    {
        case proto::system_info::Services::Service::STARTUP_TYPE_AUTO_START:
            return tr("Auto Start");

        case proto::system_info::Services::Service::STARTUP_TYPE_DEMAND_START:
            return tr("Demand Start");

        case proto::system_info::Services::Service::STARTUP_TYPE_DISABLED:
            return tr("Disabled");

        case proto::system_info::Services::Service::STARTUP_TYPE_BOOT_START:
            return tr("Boot Start");

        case proto::system_info::Services::Service::STARTUP_TYPE_SYSTEM_START:
            return tr("System Start");

        default:
            return tr("Unknown");
    }
}

} // namespace client
