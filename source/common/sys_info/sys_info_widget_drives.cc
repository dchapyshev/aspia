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

#include "common/sys_info/sys_info_widget_drives.h"

#include <QMenu>

#include "common/system_info_constants.h"
#include "common/desktop/formatter.h"
#include "proto/system_info.h"
#include "ui_sys_info_widget_drives.h"

namespace {

const char kDriveIcon[] = ":/img/hdd.svg";

class Item : public QTreeWidgetItem
{
public:
    Item(const QString& icon_path, const QString& text, const QList<QTreeWidgetItem*>& childs)
    {
        QIcon icon(icon_path);

        setIcon(0, icon);
        setText(0, text);

        for (const auto& child : childs)
            child->setIcon(0, icon);

        addChildren(childs);
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
QString busTypeToString(proto::system_info::PhysicalDrives::Drive::BusType bus_type)
{
    using Drive = proto::system_info::PhysicalDrives::Drive;

    switch (bus_type)
    {
        case Drive::BUS_TYPE_SCSI:                return "SCSI";
        case Drive::BUS_TYPE_ATAPI:               return "ATAPI";
        case Drive::BUS_TYPE_ATA:                 return "ATA";
        case Drive::BUS_TYPE_IEEE1394:            return "IEEE 1394";
        case Drive::BUS_TYPE_SSA:                 return "SSA";
        case Drive::BUS_TYPE_FIBRE:               return "Fibre Channel";
        case Drive::BUS_TYPE_USB:                 return "USB";
        case Drive::BUS_TYPE_RAID:                return "RAID";
        case Drive::BUS_TYPE_ISCSI:               return "iSCSI";
        case Drive::BUS_TYPE_SAS:                 return "SAS";
        case Drive::BUS_TYPE_SATA:                return "SATA";
        case Drive::BUS_TYPE_SD:                  return "SD";
        case Drive::BUS_TYPE_MMC:                 return "MMC";
        case Drive::BUS_TYPE_VIRTUAL:             return "Virtual";
        case Drive::BUS_TYPE_FILE_BACKED_VIRTUAL: return "File Backed Virtual";
        case Drive::BUS_TYPE_NVME:                return "NVMe";
        case Drive::BUS_TYPE_SPACES:              return "Storage Spaces";
        case Drive::BUS_TYPE_SCM:                 return "SCM";
        case Drive::BUS_TYPE_UFS:                 return "UFS";
        case Drive::BUS_TYPE_NVME_OF:             return "NVMe-oF";
        default:                                  return QString();
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetDrives::SysInfoWidgetDrives(QWidget* parent)
    : SysInfoWidget(parent),
      ui(std::make_unique<Ui::SysInfoDrives>())
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
            this, &SysInfoWidgetDrives::onContextMenu);

    connect(ui->tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetDrives::~SysInfoWidgetDrives() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetDrives::category() const
{
    return kSystemInfo_Drives;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrives::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui->tree->clear();

    // A report arrives without the drives when the enumeration found none, and one that found none
    // was possibly just not allowed to look. The next report may still bring them.
    const bool has_drives = system_info.has_physical_drives();

    ui->tree->setEnabled(has_drives);

    if (!has_drives)
        return;

    using ProtoDrive = proto::system_info::PhysicalDrives::Drive;

    const proto::system_info::PhysicalDrives& drives = system_info.physical_drives();

    for (int i = 0; i < drives.drive_size(); ++i)
    {
        const ProtoDrive& drive = drives.drive(i);
        QList<QTreeWidgetItem*> group;

        if (!drive.path().empty())
            group << mk(tr("Path"), QString::fromStdString(drive.path()));

        if (!drive.model().empty())
            group << mk(tr("Model"), QString::fromStdString(drive.model()));

        if (!drive.serial_number().empty())
            group << mk(tr("Serial Number"), QString::fromStdString(drive.serial_number()));

        if (!drive.firmware_revision().empty())
            group << mk(tr("Firmware Revision"), QString::fromStdString(drive.firmware_revision()));

        const QString bus_type = busTypeToString(drive.bus_type());
        if (!bus_type.isEmpty())
            group << mk(tr("Bus Type"), bus_type);

        if (drive.size())
            group << mk(tr("Size"), Formatter::sizeToString(drive.size()));

        // Neither the operating system nor the drive itself is obliged to name the media.
        if (drive.media_type() != ProtoDrive::MEDIA_TYPE_UNKNOWN)
        {
            group << mk(tr("Media Type"),
                        drive.media_type() == ProtoDrive::MEDIA_TYPE_SOLID_STATE ?
                            tr("Solid State") : tr("Rotating"));
        }

        if (drive.rotation_rate())
            group << mk(tr("Rotation Rate"), tr("%1 RPM").arg(drive.rotation_rate()));

        if (drive.buffer_size())
        {
            group << mk(tr("Cache Size"),
                        Formatter::sizeToString(drive.buffer_size()));
        }

        group << mk(tr("Removable"), drive.removable() ? tr("Yes") : tr("No"));

        const QString title = drive.model().empty() ?
            QString::fromStdString(drive.path()) :
            QString::fromStdString(drive.model());

        QTreeWidgetItem* item = new Item(kDriveIcon, title, group);

        ui->tree->addTopLevelItem(item);
        item->setExpanded(true);
    }

    if (!isStateRestored())
        ui->tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetDrives::treeWidget()
{
    return ui->tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrives::onContextMenu(const QPoint& point)
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
