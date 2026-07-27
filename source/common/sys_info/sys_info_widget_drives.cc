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

#include "base/drive_smart.h"
#include "base/time_types.h"
#include "common/system_info_constants.h"
#include "common/desktop/formatter.h"
#include "proto/system_info.h"
#include "ui_sys_info_widget_drives.h"

namespace {

const char kDriveIcon[] = ":/img/hdd.svg";

// Index of the drive a top level item of the upper pane stands for.
constexpr int kDriveIndexRole = Qt::UserRole;

// One data unit an NVMe drive reports is 1000 blocks of 512 bytes.
constexpr quint64 kNvmeDataUnitSize = 1000 * 512;

// A drive reports its temperature in Kelvin. Values outside of this range are not believable and are
// treated as the drive not reporting a temperature at all.
constexpr quint32 kMinTemperature = 173; // -100 degrees Celsius.
constexpr quint32 kMaxTemperature = 473; // 200 degrees Celsius.

// Number of hex digits a raw value is padded to. An ATA attribute carries a 48-bit counter, an NVMe
// counter is shown as wide as the tools that read such drives show it.
constexpr int kAtaRawDigits = 12;
constexpr int kNvmeRawDigits = 14;

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
// Row of the health data of an NVMe drive. Such a drive has no normalized values, so the readable
// value goes into the value column and the number the drive reported into the raw column.
QTreeWidgetItem* mkCounter(const QString& param, const QString& value, const QString& raw)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();

    item->setText(0, param);
    item->setText(1, value);
    item->setText(2, raw);

    return item;
}

//--------------------------------------------------------------------------------------------------
// Raw counters are shown as zero padded uppercase hex, the way S.M.A.R.T. tools show them.
QString rawToString(quint64 raw, int digits)
{
    return QString("%1").arg(raw, digits, 16, QChar('0')).toUpper();
}

//--------------------------------------------------------------------------------------------------
bool isTemperatureReported(quint32 kelvin)
{
    return kelvin >= kMinTemperature && kelvin <= kMaxTemperature;
}

//--------------------------------------------------------------------------------------------------
int kelvinToCelsius(quint32 kelvin)
{
    return static_cast<int>(kelvin) - 273;
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

//--------------------------------------------------------------------------------------------------
// Names of the widely used attribute identifiers. A vendor is free to give an identifier its own
// meaning, so an unexpected name for an exotic drive is possible.
const char* attributeName(quint32 id)
{
    switch (id)
    {
        case 0x01: return "Raw Read Error Rate";
        case 0x02: return "Throughput Performance";
        case 0x03: return "Spin Up Time";
        case 0x04: return "Start/Stop Count";
        case 0x05: return "Reallocated Sectors Count";
        case 0x06: return "Read Channel Margin";
        case 0x07: return "Seek Error Rate";
        case 0x08: return "Seek Time Performance";
        case 0x09: return "Power-On Hours";
        case 0x0A: return "Spin Retry Count";
        case 0x0B: return "Recalibration Retries";
        case 0x0C: return "Device Power Cycle Count";
        case 0x0D: return "Soft Read Error Rate";
        case 0x16: return "Current Helium Level";
        case 0xAA: return "Available Reserved Space";
        case 0xAB: return "Program Fail Count";
        case 0xAC: return "Erase Fail Count";
        case 0xAD: return "Wear Leveling Count";
        case 0xAE: return "Unexpected Power Loss Count";
        case 0xAF: return "Power Loss Protection Failure";
        case 0xB0: return "Erase Fail Count";
        case 0xB1: return "Wear Range Delta";
        case 0xB2: return "Used Reserved Block Count";
        case 0xB3: return "Used Reserved Block Count Total";
        case 0xB4: return "Unused Reserved Block Count Total";
        case 0xB5: return "Program Fail Count Total";
        case 0xB6: return "Erase Fail Count Total";
        case 0xB7: return "SATA Downshift Error Count";
        case 0xB8: return "End To End Error";
        case 0xB9: return "Head Stability";
        case 0xBA: return "Induced Op Vibration Detection";
        case 0xBB: return "Reported Uncorrectable Errors";
        case 0xBC: return "Command Timeout";
        case 0xBD: return "High Fly Writes";
        case 0xBE: return "Temperature Difference From 100";
        case 0xBF: return "G-Sense Error Rate";
        case 0xC0: return "Emergency Retract Cycle Count";
        case 0xC1: return "Load/Unload Cycle Count";
        case 0xC2: return "Temperature";
        case 0xC3: return "Hardware ECC Recovered";
        case 0xC4: return "Reallocation Event Count";
        case 0xC5: return "Current Pending Sector Count";
        case 0xC6: return "Uncorrectable Sector Count";
        case 0xC7: return "UltraDMA CRC Error Count";
        case 0xC8: return "Write Error Rate";
        case 0xC9: return "Soft Read Error Rate";
        case 0xCA: return "Data Address Mark Errors";
        case 0xCB: return "Run Out Cancel";
        case 0xCC: return "Soft ECC Correction";
        case 0xCD: return "Thermal Asperity Rate";
        case 0xCE: return "Flying Height";
        case 0xCF: return "Spin High Current";
        case 0xD0: return "Spin Buzz";
        case 0xD1: return "Offline Seek Performance";
        case 0xD2: return "Vibration During Write";
        case 0xD3: return "Vibration During Write";
        case 0xD4: return "Shock During Write";
        case 0xDC: return "Disk Shift";
        case 0xDD: return "G-Sense Error Rate";
        case 0xDE: return "Loaded Hours";
        case 0xDF: return "Load/Unload Retry Count";
        case 0xE0: return "Load Friction";
        case 0xE1: return "Load/Unload Cycle Count";
        case 0xE2: return "Load-In Time";
        case 0xE3: return "Torque Amplification Count";
        case 0xE4: return "Power-Off Retract Count";
        case 0xE6: return "GMR Head Amplitude";
        case 0xE7: return "Temperature";
        case 0xE8: return "Endurance Remaining";
        case 0xE9: return "Media Wearout Indicator";
        case 0xEA: return "Average Erase Count";
        case 0xEB: return "Good Block Count";
        case 0xF0: return "Head Flying Hours";
        case 0xF1: return "Total LBAs Written";
        case 0xF2: return "Total LBAs Read";
        case 0xF3: return "Total LBAs Written Expanded";
        case 0xF4: return "Total LBAs Read Expanded";
        case 0xF9: return "NAND Writes";
        case 0xFA: return "Read Error Retry Rate";
        case 0xFB: return "Minimum Spares Remaining";
        case 0xFC: return "Newly Added Bad Flash Block";
        case 0xFE: return "Free Fall Event Count";
        default:   return nullptr;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetDrives::SysInfoWidgetDrives(QWidget* parent)
    : SysInfoWidget(parent),
      ui(std::make_unique<Ui::SysInfoDrives>())
{
    ui->setupUi(this);

    QList<int> sizes;
    sizes.emplace_back(250);
    sizes.emplace_back(250);
    ui->splitter->setSizes(sizes);

    connect(ui->action_copy_row, &QAction::triggered, this, [this]()
    {
        if (context_tree_)
            copyRow(context_tree_->currentItem());
    });

    connect(ui->action_copy_name, &QAction::triggered, this, [this]()
    {
        if (context_tree_)
            copyColumn(context_tree_->currentItem(), 0);
    });

    connect(ui->action_copy_value, &QAction::triggered, this, [this]()
    {
        if (context_tree_)
            copyColumn(context_tree_->currentItem(), 1);
    });

    connect(ui->tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetDrives::onContextMenu);
    connect(ui->tree_health, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetDrives::onHealthContextMenu);

    connect(ui->tree, &QTreeWidget::currentItemChanged,
            this, &SysInfoWidgetDrives::onCurrentDriveChanged);

    connect(ui->tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });

    connect(ui->tree_health, &QTreeWidget::itemDoubleClicked,
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
    ui->tree_health->clear();
    drives_.reset();

    if (!system_info.has_physical_drives())
    {
        ui->tree->setEnabled(false);
        ui->tree_health->setEnabled(false);
        return;
    }

    drives_ = std::make_unique<proto::system_info::PhysicalDrives>(system_info.physical_drives());

    using ProtoDrive = proto::system_info::PhysicalDrives::Drive;

    for (int i = 0; i < drives_->drive_size(); ++i)
    {
        const ProtoDrive& drive = drives_->drive(i);
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
        item->setData(0, kDriveIndexRole, i);

        ui->tree->addTopLevelItem(item);
        item->setExpanded(true);
    }

    if (!isStateRestored())
        ui->tree->resizeColumnToContents(0);

    // Selecting the first drive fills the lower pane.
    if (ui->tree->topLevelItemCount())
        ui->tree->setCurrentItem(ui->tree->topLevelItem(0));
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetDrives::treeWidget()
{
    return ui->tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrives::onContextMenu(const QPoint& point)
{
    showContextMenu(ui->tree, point);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrives::onHealthContextMenu(const QPoint& point)
{
    showContextMenu(ui->tree_health, point);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrives::onCurrentDriveChanged()
{
    ui->tree_health->clear();

    QTreeWidgetItem* item = ui->tree->currentItem();
    if (!item || !drives_)
        return;

    // A property of a drive is selected just as well as the drive itself.
    while (item->parent())
        item = item->parent();

    const int drive_index = item->data(0, kDriveIndexRole).toInt();
    if (drive_index < 0 || drive_index >= drives_->drive_size())
        return;

    const proto::system_info::PhysicalDrives::Drive& drive = drives_->drive(drive_index);

    if (drive.smart_attribute_size())
    {
        setAtaHealth(drive_index);
    }
    else if (drive.has_nvme_health())
    {
        setNvmeHealth(drive_index);
    }
    else
    {
        ui->tree_health->setColumnCount(2);
        ui->tree_health->setHeaderLabels({ tr("Parameter"), tr("Value") });
        ui->tree_health->addTopLevelItem(mk(tr("Health Data"), tr("Not available")));
    }

    const QIcon icon(kDriveIcon);

    for (int i = 0; i < ui->tree_health->topLevelItemCount(); ++i)
        ui->tree_health->topLevelItem(i)->setIcon(0, icon);

    for (int i = 0; i < ui->tree_health->columnCount(); ++i)
        ui->tree_health->resizeColumnToContents(i);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrives::showContextMenu(QTreeWidget* tree, const QPoint& point)
{
    QTreeWidgetItem* current_item = tree->itemAt(point);
    if (!current_item)
        return;

    tree->setCurrentItem(current_item);
    context_tree_ = tree;

    QMenu menu;
    menu.addAction(ui->action_copy_row);
    menu.addAction(ui->action_copy_name);
    menu.addAction(ui->action_copy_value);

    menu.exec(tree->viewport()->mapToGlobal(point));
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrives::setAtaHealth(int drive_index)
{
    ui->tree_health->setColumnCount(6);
    ui->tree_health->setHeaderLabels({ tr("Attribute"), tr("Value"), tr("Worst"), tr("Threshold"),
                                       tr("Raw"), tr("Status") });

    const proto::system_info::PhysicalDrives::Drive& drive = drives_->drive(drive_index);

    for (int i = 0; i < drive.smart_attribute_size(); ++i)
    {
        const proto::system_info::PhysicalDrives::Drive::SmartAttribute& attribute =
            drive.smart_attribute(i);

        QString status;
        if (!attribute.threshold())
            status = tr("OK. Always passed");
        else if (attribute.value() > attribute.threshold())
            status = tr("OK. Value is normal");
        else if (attribute.status_flags() & AtaSmart::STATUS_FLAG_PRE_FAILURE)
            status = tr("Warning. Value is pre-failure");
        else
            status = tr("Warning. Value is not normal");

        const char* name = attributeName(attribute.id());
        const QString id = QString("%1").arg(attribute.id(), 2, 16, QChar('0')).toUpper();

        QTreeWidgetItem* item = new QTreeWidgetItem();

        item->setText(0, QString("%1 %2").arg(id, name ? name : tr("Unknown Attribute")));
        item->setText(1, QString::number(attribute.value()));
        item->setText(2, QString::number(attribute.worst_value()));
        item->setText(3, QString::number(attribute.threshold()));
        item->setText(4, rawToString(attribute.raw(), kAtaRawDigits));
        item->setText(5, status);

        ui->tree_health->addTopLevelItem(item);
    }
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDrives::setNvmeHealth(int drive_index)
{
    ui->tree_health->setColumnCount(3);
    ui->tree_health->setHeaderLabels({ tr("Parameter"), tr("Value"), tr("Raw") });

    const proto::system_info::PhysicalDrives::Drive::NvmeHealth& health =
        drives_->drive(drive_index).nvme_health();

    QStringList warnings;
    const quint32 critical_warning = health.critical_warning();

    if (critical_warning & NvmeSmart::CRITICAL_WARNING_SPARE_BELOW_THRESHOLD)
        warnings << tr("Spare capacity is below the threshold");
    if (critical_warning & NvmeSmart::CRITICAL_WARNING_TEMPERATURE)
        warnings << tr("Temperature is outside of the operating range");
    if (critical_warning & NvmeSmart::CRITICAL_WARNING_RELIABILITY_DEGRADED)
        warnings << tr("Reliability is degraded");
    if (critical_warning & NvmeSmart::CRITICAL_WARNING_READ_ONLY)
        warnings << tr("Media is in read-only mode");
    if (critical_warning & NvmeSmart::CRITICAL_WARNING_VOLATILE_BACKUP_FAILED)
        warnings << tr("Volatile memory backup device failed");
    if (critical_warning & NvmeSmart::CRITICAL_WARNING_PERSISTENT_MEMORY_ERROR)
        warnings << tr("Persistent memory region is unreliable");

    QList<QTreeWidgetItem*> rows;

    rows << mkCounter(tr("Critical Warning"),
                      warnings.isEmpty() ? tr("None") : warnings.join("; "),
                      rawToString(critical_warning, kNvmeRawDigits));

    if (isTemperatureReported(health.composite_temperature()))
    {
        rows << mkCounter(tr("Temperature"),
                          tr("%1 C").arg(kelvinToCelsius(health.composite_temperature())),
                          rawToString(health.composite_temperature(), kNvmeRawDigits));
    }

    for (int i = 0; i < health.temperature_sensor_size(); ++i)
    {
        const quint32 temperature = health.temperature_sensor(i);
        if (!isTemperatureReported(temperature))
            continue;

        rows << mkCounter(tr("Temperature Sensor %1").arg(i + 1),
                          tr("%1 C").arg(kelvinToCelsius(temperature)),
                          rawToString(temperature, kNvmeRawDigits));
    }

    rows << mkCounter(tr("Available Spare"), tr("%1%").arg(health.available_spare()),
                      rawToString(health.available_spare(), kNvmeRawDigits));
    rows << mkCounter(tr("Available Spare Threshold"),
                      tr("%1%").arg(health.available_spare_threshold()),
                      rawToString(health.available_spare_threshold(), kNvmeRawDigits));
    rows << mkCounter(tr("Percentage Used"), tr("%1%").arg(health.percentage_used()),
                      rawToString(health.percentage_used(), kNvmeRawDigits));

    rows << mkCounter(tr("Data Read"),
                      Formatter::sizeToString(health.data_units_read() * kNvmeDataUnitSize),
                      rawToString(health.data_units_read(), kNvmeRawDigits));
    rows << mkCounter(tr("Data Written"),
                      Formatter::sizeToString(health.data_units_written() * kNvmeDataUnitSize),
                      rawToString(health.data_units_written(), kNvmeRawDigits));

    rows << mkCounter(tr("Host Read Commands"), QString::number(health.host_read_commands()),
                      rawToString(health.host_read_commands(), kNvmeRawDigits));
    rows << mkCounter(tr("Host Write Commands"), QString::number(health.host_write_commands()),
                      rawToString(health.host_write_commands(), kNvmeRawDigits));
    rows << mkCounter(tr("Controller Busy Time"),
                      Formatter::delayToString(Minutes(health.controller_busy_time())),
                      rawToString(health.controller_busy_time(), kNvmeRawDigits));
    rows << mkCounter(tr("Power Cycles"), QString::number(health.power_cycles()),
                      rawToString(health.power_cycles(), kNvmeRawDigits));
    rows << mkCounter(tr("Power-On Time"),
                      Formatter::delayToString(Hours(health.power_on_hours())),
                      rawToString(health.power_on_hours(), kNvmeRawDigits));
    rows << mkCounter(tr("Unsafe Shutdowns"), QString::number(health.unsafe_shutdowns()),
                      rawToString(health.unsafe_shutdowns(), kNvmeRawDigits));
    rows << mkCounter(tr("Media Errors"), QString::number(health.media_errors()),
                      rawToString(health.media_errors(), kNvmeRawDigits));
    rows << mkCounter(tr("Error Log Entries"), QString::number(health.error_log_entries()),
                      rawToString(health.error_log_entries(), kNvmeRawDigits));

    rows << mkCounter(tr("Warning Temperature Time"),
                      Formatter::delayToString(Minutes(health.warning_temperature_time())),
                      rawToString(health.warning_temperature_time(), kNvmeRawDigits));
    rows << mkCounter(tr("Critical Temperature Time"),
                      Formatter::delayToString(Minutes(health.critical_temperature_time())),
                      rawToString(health.critical_temperature_time(), kNvmeRawDigits));

    ui->tree_health->addTopLevelItems(rows);
}
