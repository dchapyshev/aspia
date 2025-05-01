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

#include "client/ui/sys_info/sys_info_widget_summary.h"

#include "base/macros_magic.h"
#include "common/system_info_constants.h"

#include <QMenu>

namespace client {

namespace {

class Item : public QTreeWidgetItem
{
public:
    Item(const char* icon_path, const QString& text, const QList<QTreeWidgetItem*>& childs)
    {
        QIcon icon(icon_path);

        setIcon(0, icon);
        setText(0, text);

        for (const auto& child : childs)
        {
            child->setIcon(0, icon);

            for (int i = 0; i < child->childCount(); ++i)
            {
                QTreeWidgetItem* item = child->child(i);
                if (item)
                    item->setIcon(0, icon);
            }
        }

        addChildren(childs);
    }

    Item(const QString& text, const QList<QTreeWidgetItem*>& params)
    {
        setText(0, text);
        addChildren(params);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(Item);
};

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* mk(const QString& param, const QString& value)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();

    item->setText(0, param);
    item->setText(1, value);

    return item;
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* mk(const QString& param, const std::string& value)
{
    return mk(param, QString::fromStdString(value));
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetSummary::SysInfoWidgetSummary(QWidget* parent)
    : SysInfoWidget(parent)
{
    ui.setupUi(this);

    connect(ui.action_copy_row, &QAction::triggered, this, [this]()
    {
        copyRow(ui.tree->currentItem());
    });

    connect(ui.action_copy_name, &QAction::triggered, this, [this]()
    {
        copyColumn(ui.tree->currentItem(), 0);
    });

    connect(ui.action_copy_value, &QAction::triggered, this, [this]()
    {
        copyColumn(ui.tree->currentItem(), 1);
    });

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetSummary::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetSummary::~SysInfoWidgetSummary() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetSummary::category() const
{
    return common::kSystemInfo_Summary;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetSummary::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (system_info.has_computer())
    {
        const proto::system_info::Computer& computer = system_info.computer();
        QList<QTreeWidgetItem*> items;

        if (!computer.name().empty())
            items << mk(tr("Name"), computer.name());

        if (!computer.domain().empty())
            items << mk(tr("Domain"), computer.domain());

        if (!computer.workgroup().empty())
            items << mk(tr("Workgroup"), computer.workgroup());

        if (computer.uptime())
            items << mk(tr("Uptime"), delayToString(computer.uptime()));

        if (!items.isEmpty())
            ui.tree->addTopLevelItem(new Item(":/img/computer.png", tr("Computer"), items));
    }

    {
        QList<QTreeWidgetItem*> items;

        items << mk(tr("Host Version"), host_version_);
        items << mk(tr("Client Version"), client_version_);
        items << mk(tr("Router Version"), router_version_);

        ui.tree->addTopLevelItem(new Item(":/img/main.png", tr("Aspia Information"), items));
    }

    if (system_info.has_operating_system())
    {
        const proto::system_info::OperatingSystem& os = system_info.operating_system();
        QList<QTreeWidgetItem*> items;

        if (!os.name().empty())
            items << mk(tr("Name"), os.name());

        if (!os.version().empty())
            items << mk(tr("Version"), os.version());

        if (!os.arch().empty())
            items << mk(tr("Architecture"), os.arch());

        if (!os.key().empty())
            items << mk(tr("License Key"), os.key());

        if (os.install_date() != 0)
            items << mk(tr("Install Date"), timeToString(os.install_date()));

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/operating-system.png", tr("Operating System"), items));
        }
    }

    if (system_info.has_motherboard())
    {
        const proto::system_info::Motherboard& motherboard = system_info.motherboard();
        QList<QTreeWidgetItem*> items;

        if (!motherboard.manufacturer().empty())
            items << mk(tr("Manufacturer"), motherboard.manufacturer());

        if (!motherboard.model().empty())
            items << mk(tr("Model"), motherboard.model());

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/motherboard.png", tr("Motherboard"), items));
        }
    }

    if (system_info.has_processor())
    {
        const proto::system_info::Processor& processor = system_info.processor();
        QList<QTreeWidgetItem*> items;

        if (!processor.model().empty())
            items << mk(tr("Model"), processor.model());

        if (!processor.vendor().empty())
            items << mk(tr("Vendor"), processor.vendor());

        if (processor.packages())
            items << mk(tr("Packages"), QString::number(processor.packages()));

        if (processor.cores())
            items << mk(tr("Cores"), QString::number(processor.cores()));

        if (processor.threads())
            items << mk(tr("Threads"), QString::number(processor.threads()));

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/processor.png", tr("Processor"), items));
        }
    }

    if (system_info.has_bios())
    {
        const proto::system_info::Bios& bios = system_info.bios();
        QList<QTreeWidgetItem*> items;

        if (!bios.vendor().empty())
            items << mk(tr("Vendor"), bios.vendor());

        if (!bios.version().empty())
            items << mk(tr("Version"), bios.version());

        if (!bios.date().empty())
            items << mk(tr("Date"), bios.date());

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(new Item(":/img/bios.png", "BIOS", items));
        }
    }

    if (system_info.has_memory())
    {
        const proto::system_info::Memory& memory = system_info.memory();
        QList<QTreeWidgetItem*> items;

        for (int i = 0; i < memory.module_size(); ++i)
        {
            const proto::system_info::Memory::Module& module = memory.module(i);
            QList<QTreeWidgetItem*> group;

            if (module.present())
            {
                if (!module.manufacturer().empty())
                    group << mk(tr("Manufacturer"), module.manufacturer());

                if (module.size())
                    group << mk(tr("Size"), sizeToString(static_cast<int64_t>(module.size())));

                if (module.speed())
                    group << mk(tr("Speed"), tr("%1 MHz").arg(module.speed()));

                if (!module.type().empty())
                    group << mk(tr("Type"), module.type());

                if (!module.form_factor().empty())
                    group << mk(tr("Form Factor"), module.form_factor());

                if (!module.part_number().empty())
                    group << mk(tr("Part Number"), module.part_number());
            }
            else
            {
                group << mk(tr("Installed"), tr("No"));
            }

            if (!group.isEmpty())
                items << new Item(QString::fromStdString(module.location()), group);
        }

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(":/img/memory.png", tr("Memory"), items));
        }
    }

    if (system_info.has_logical_drives())
    {
        const proto::system_info::LogicalDrives& drives = system_info.logical_drives();
        QList<QTreeWidgetItem*> items;

        for (int i = 0; i < drives.drive_size(); ++i)
        {
            const proto::system_info::LogicalDrives::Drive& drive = drives.drive(i);

            QString param;
            QString value;

            if (drive.file_system().empty())
            {
                param = QString::fromStdString(drive.path());
            }
            else
            {
                param = QString("%1 (%2)")
                    .arg(QString::fromStdString(drive.path()),
                         QString::fromStdString(drive.file_system()));
            }

            if (drive.total_size() && drive.total_size() != static_cast<quint64>(-1))
            {
                value = tr("%1 (%2 free)")
                    .arg(sizeToString(static_cast<int64_t>(drive.total_size())),
                         sizeToString(static_cast<int64_t>(drive.free_size())));
            }

            items << mk(param, value);
        }

        if (!items.isEmpty())
        {
            ui.tree->addTopLevelItem(new Item(":/img/drive.png", tr("Logical Drives"), items));
        }
    }

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
        ui.tree->topLevelItem(i)->setExpanded(true);

    ui.tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetSummary::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetSummary::setRouterVersion(const QVersionNumber& router_version)
{
    if (!router_version.isNull())
        router_version_ = router_version.toString();
    else
        router_version_ = tr("No");
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetSummary::setHostVersion(const QVersionNumber& host_version)
{
    host_version_ = host_version.toString();
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetSummary::setClientVersion(const QVersionNumber& client_version)
{
    client_version_ = client_version.toString();
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetSummary::onContextMenu(const QPoint& point)
{
    QTreeWidgetItem* current_item = ui.tree->itemAt(point);
    if (!current_item)
        return;

    ui.tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui.action_copy_row);
    menu.addAction(ui.action_copy_name);
    menu.addAction(ui.action_copy_value);

    menu.exec(ui.tree->viewport()->mapToGlobal(point));
}

} // namespace client
