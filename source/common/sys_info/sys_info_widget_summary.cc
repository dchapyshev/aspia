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

#include "common/sys_info/sys_info_widget_summary.h"

#include <QMenu>

#include "common/system_info_constants.h"
#include "common/desktop/formatter.h"
#include "proto/system_info.h"
#include "ui_sys_info_widget_summary.h"

namespace {

class Item : public QTreeWidgetItem
{
public:
    Item(const QString& icon_path, const QString& text, const QList<QTreeWidgetItem*>& childs)
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
    Q_DISABLE_COPY_MOVE(Item)
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
    : SysInfoWidget(parent),
      ui(std::make_unique<Ui::SysInfoSummary>())
{
    ui->setupUi(this);

    connect(ui->action_copy_row, &QAction::triggered, this, [this]()
    {
        copyRow(ui->tree->currentItem());
    });

    connect(ui->action_copy_name, &QAction::triggered, this, [this]()
    {
        copyColumn(ui->tree->currentItem(), 0);
    });

    connect(ui->action_copy_value, &QAction::triggered, this, [this]()
    {
        copyColumn(ui->tree->currentItem(), 1);
    });

    connect(ui->tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetSummary::onContextMenu);

    connect(ui->tree, &QTreeWidget::itemDoubleClicked,
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
    return kSystemInfo_Summary;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetSummary::setSystemInfo(const proto::system_info::SystemInfo& report)
{
    bool changed = false;

    if (report.has_computer())
    {
        computer_ = report.computer();
        changed = true;
    }

    if (report.has_operating_system())
    {
        operating_system_ = report.operating_system();
        changed = true;
    }

    if (report.has_processor())
    {
        processor_ = report.processor();
        changed = true;
    }

    if (report.has_dmi())
    {
        dmi_ = report.dmi();
        changed = true;
    }

    if (report.has_logical_drives())
    {
        logical_drives_ = report.logical_drives();
        changed = true;
    }

    if (!changed)
        return;

    ui->tree->clear();

    if (computer_)
    {
        const proto::system_info::Computer& computer = *computer_;
        QList<QTreeWidgetItem*> items;

        if (!computer.name().empty())
            items << mk(tr("Name"), computer.name());

        if (!computer.domain().empty())
            items << mk(tr("Domain"), computer.domain());

        if (!computer.workgroup().empty())
            items << mk(tr("Workgroup"), computer.workgroup());

        if (computer.uptime())
            items << mk(tr("Uptime"), Formatter::delayToString(Seconds(computer.uptime())));

        if (!items.isEmpty())
            ui->tree->addTopLevelItem(new Item(":/img/computer.svg", tr("Computer"), items));
    }

    {
        QList<QTreeWidgetItem*> items;

        if (!host_version_.isEmpty())
            items << mk(tr("Host Version"), host_version_);

        if (!client_version_.isEmpty())
            items << mk(tr("Client Version"), client_version_);

        // Which router the session went through is only a question where there is a session at all,
        // and the client is the only side that names itself.
        if (!router_version_.isEmpty())
            items << mk(tr("Router Version"), router_version_);
        else if (!client_version_.isEmpty())
            items << mk(tr("Router Version"), tr("No"));

        if (!items.isEmpty())
            ui->tree->addTopLevelItem(new Item(":/img/info.svg", tr("Aspia Information"), items));
    }

    if (operating_system_)
    {
        const proto::system_info::OperatingSystem& os = *operating_system_;
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
            items << mk(tr("Install Date"), Formatter::timeToString(os.install_date()));

        if (!items.isEmpty())
        {
            ui->tree->addTopLevelItem(
                new Item(":/img/operating-system.svg", tr("Operating System"), items));
        }
    }

    if (dmi_ && dmi_->baseboard_size())
    {
        const proto::system_info::Dmi::Baseboard& baseboard = dmi_->baseboard(0);
        QList<QTreeWidgetItem*> items;

        if (!baseboard.manufacturer().empty())
            items << mk(tr("Manufacturer"), baseboard.manufacturer());

        if (!baseboard.product().empty())
            items << mk(tr("Model"), baseboard.product());

        if (!items.isEmpty())
        {
            ui->tree->addTopLevelItem(
                new Item(":/img/motherboard.svg", tr("Motherboard"), items));
        }
    }

    if (processor_)
    {
        const proto::system_info::Processor& processor = *processor_;
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
            ui->tree->addTopLevelItem(
                new Item(":/img/microchip.svg", tr("Processor"), items));
        }
    }

    if (dmi_ && dmi_->bios_size())
    {
        const proto::system_info::Dmi::Bios& bios = dmi_->bios(0);
        QList<QTreeWidgetItem*> items;

        if (!bios.vendor().empty())
            items << mk(tr("Vendor"), bios.vendor());

        if (!bios.version().empty())
            items << mk(tr("Version"), bios.version());

        if (!bios.release_date().empty())
            items << mk(tr("Date"), bios.release_date());

        if (!items.isEmpty())
        {
            ui->tree->addTopLevelItem(new Item(":/img/processor.svg", "BIOS", items));
        }
    }

    if (dmi_ && dmi_->memory_device_size())
    {
        QList<QTreeWidgetItem*> items;

        for (int i = 0; i < dmi_->memory_device_size(); ++i)
        {
            const proto::system_info::Dmi::MemoryDevice& module = dmi_->memory_device(i);
            QList<QTreeWidgetItem*> group;

            // The summary names the module, the whole of what it reports is on the DMI page.
            if (module.present())
            {
                if (!module.manufacturer().empty())
                    group << mk(tr("Manufacturer"), module.manufacturer());

                if (module.size())
                    group << mk(tr("Size"), Formatter::sizeToString(module.size()));

                if (!module.type().empty())
                    group << mk(tr("Type"), module.type());

                if (module.speed())
                    group << mk(tr("Speed"), tr("%1 MT/s").arg(module.speed()));
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
            ui->tree->addTopLevelItem(
                new Item(":/img/memory-slot.svg", tr("Memory"), items));
        }
    }

    if (logical_drives_)
    {
        const proto::system_info::LogicalDrives& drives = *logical_drives_;
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
                    .arg(Formatter::sizeToString(drive.total_size()),
                         Formatter::sizeToString(drive.free_size()));
            }

            items << mk(param, value);
        }

        if (!items.isEmpty())
        {
            ui->tree->addTopLevelItem(new Item(":/img/hdd.svg", tr("Logical Drives"), items));
        }
    }

    for (int i = 0; i < ui->tree->topLevelItemCount(); ++i)
        ui->tree->topLevelItem(i)->setExpanded(true);

    if (!isStateRestored())
        ui->tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetSummary::treeWidget()
{
    return ui->tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetSummary::setRouterVersion(const QVersionNumber& router_version)
{
    router_version_ = router_version.toString();
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
    QTreeWidgetItem* current_item = ui->tree->itemAt(point);
    if (!current_item)
        return;

    ui->tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui->action_copy_row);
    menu.addAction(ui->action_copy_name);
    menu.addAction(ui->action_copy_value);

    menu.exec(ui->tree->viewport()->mapToGlobal(point));
}
