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

#include "common/sys_info/sys_info_widget_dmi.h"

#include <QMenu>

#include "common/system_info_constants.h"
#include "common/desktop/formatter.h"
#include "proto/system_info.h"
#include "ui_sys_info_widget_dmi.h"

namespace {

const char kBiosIcon[] = ":/img/processor.svg";
const char kBaseboardIcon[] = ":/img/motherboard.svg";
const char kChassisIcon[] = ":/img/computer-case.svg";
const char kProcessorIcon[] = ":/img/microchip.svg";
const char kCacheIcon[] = ":/img/microchip.svg";
const char kPortConnectorIcon[] = ":/img/ps-2-male.svg";
const char kSystemSlotIcon[] = ":/img/ps-2-male.svg";
const char kOnBoardDeviceIcon[] = ":/img/network-card.svg";
const char kOemStringsIcon[] = ":/img/file-document.svg";
const char kConfigurationOptionIcon[] = ":/img/gear.svg";
const char kSystemBootIcon[] = ":/img/restart.svg";
const char kMemoryArrayIcon[] = ":/img/memory-slot.svg";
const char kMemoryDeviceIcon[] = ":/img/memory-slot.svg";
const char kMemoryErrorIcon[] = ":/img/memory-slot.svg";
const char kMemoryArrayAddressIcon[] = ":/img/memory-slot.svg";
const char kMemoryDeviceAddressIcon[] = ":/img/memory-slot.svg";
const char kTpmDeviceIcon[] = ":/img/certificate.svg";
const char kMiscIcon[] = ":/img/info.svg";

// Group of tables a item of the upper pane belongs to.
constexpr int kGroupRole = Qt::UserRole;

// Index of the entry inside its group. The item standing for the group itself carries -1.
constexpr int kIndexRole = Qt::UserRole + 1;

enum Group
{
    GROUP_BIOS = 0,
    GROUP_BASEBOARD,
    GROUP_CHASSIS,
    GROUP_PROCESSORS,
    GROUP_CACHES,
    GROUP_PORT_CONNECTORS,
    GROUP_SYSTEM_SLOTS,
    GROUP_ON_BOARD_DEVICES,
    GROUP_OEM_STRINGS,
    GROUP_CONFIGURATION_OPTIONS,
    GROUP_MEMORY_ARRAYS,
    GROUP_MEMORY_DEVICES,
    GROUP_MEMORY_ERRORS,
    GROUP_MEMORY_ARRAY_ADDRESSES,
    GROUP_MEMORY_DEVICE_ADDRESSES,
    GROUP_SYSTEM_BOOT,
    GROUP_TPM_DEVICES,
    GROUP_MISC
};

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
                child->child(i)->setIcon(0, icon);
        }

        addChildren(childs);
    }

    // Group of parameters inside the parameters of a single table.
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

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetDmi::SysInfoWidgetDmi(QWidget* parent)
    : SysInfoWidget(parent),
      ui(std::make_unique<Ui::SysInfoDmi>())
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
            this, &SysInfoWidgetDmi::onContextMenu);
    connect(ui->tree_params, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetDmi::onParametersContextMenu);

    connect(ui->tree, &QTreeWidget::currentItemChanged,
            this, &SysInfoWidgetDmi::onCurrentTableChanged);

    connect(ui->tree_params, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetDmi::~SysInfoWidgetDmi() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetDmi::category() const
{
    return kSystemInfo_Dmi;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui->tree->clear();
    ui->tree->setRootIsDecorated(false);
    ui->tree_params->clear();
    dmi_.reset();

    // Firmware without the tables at all, or one the reader was not allowed to read, arrives as a
    // report without them. The next report may still bring them.
    const bool has_dmi = system_info.has_dmi();

    ui->tree->setEnabled(has_dmi);
    ui->tree_params->setEnabled(has_dmi);

    if (!has_dmi)
        return;

    dmi_ = std::make_unique<proto::system_info::Dmi>(system_info.dmi());

    QStringList bios;
    for (int i = 0; i < dmi_->bios_size(); ++i)
        bios << biosTitle(i);

    if (!bios.isEmpty())
        addGroup(GROUP_BIOS, kBiosIcon, tr("BIOS"), bios);

    QStringList baseboards;
    for (int i = 0; i < dmi_->baseboard_size(); ++i)
        baseboards << baseboardTitle(i);

    if (!baseboards.isEmpty())
        addGroup(GROUP_BASEBOARD, kBaseboardIcon, tr("Motherboard"), baseboards);

    QStringList chassis;
    for (int i = 0; i < dmi_->chassis_size(); ++i)
        chassis << chassisTitle(i);

    if (!chassis.isEmpty())
        addGroup(GROUP_CHASSIS, kChassisIcon, tr("Chassis"), chassis);

    QStringList processors;
    for (int i = 0; i < dmi_->processor_size(); ++i)
        processors << processorTitle(i);

    if (!processors.isEmpty())
        addGroup(GROUP_PROCESSORS, kProcessorIcon, tr("Processors"), processors);

    QStringList caches;
    for (int i = 0; i < dmi_->cache_size(); ++i)
        caches << cacheTitle(i);

    if (!caches.isEmpty())
        addGroup(GROUP_CACHES, kCacheIcon, tr("Caches"), caches);

    QStringList port_connectors;
    for (int i = 0; i < dmi_->port_connector_size(); ++i)
        port_connectors << portConnectorTitle(i);

    if (!port_connectors.isEmpty())
    {
        addGroup(GROUP_PORT_CONNECTORS, kPortConnectorIcon, tr("Port Connectors"),
                 port_connectors);
    }

    QStringList system_slots;
    for (int i = 0; i < dmi_->system_slot_size(); ++i)
        system_slots << systemSlotTitle(i);

    if (!system_slots.isEmpty())
        addGroup(GROUP_SYSTEM_SLOTS, kSystemSlotIcon, tr("System Slots"), system_slots);

    QStringList on_board_devices;
    for (int i = 0; i < dmi_->on_board_device_size(); ++i)
        on_board_devices << onBoardDeviceTitle(i);

    if (!on_board_devices.isEmpty())
    {
        addGroup(GROUP_ON_BOARD_DEVICES, kOnBoardDeviceIcon, tr("On-board Devices"),
                 on_board_devices);
    }

    if (dmi_->oem_string_size())
        addGroup(GROUP_OEM_STRINGS, kOemStringsIcon, tr("OEM Strings"), { tr("OEM Strings") });

    if (dmi_->configuration_option_size())
    {
        addGroup(GROUP_CONFIGURATION_OPTIONS, kConfigurationOptionIcon,
                 tr("Configuration Options"), { tr("Configuration Options") });
    }

    QStringList memory_arrays;
    for (int i = 0; i < dmi_->memory_array_size(); ++i)
        memory_arrays << memoryArrayTitle(i);

    if (!memory_arrays.isEmpty())
        addGroup(GROUP_MEMORY_ARRAYS, kMemoryArrayIcon, tr("Memory Arrays"), memory_arrays);

    QStringList memory_devices;
    for (int i = 0; i < dmi_->memory_device_size(); ++i)
        memory_devices << memoryDeviceTitle(i);

    if (!memory_devices.isEmpty())
    {
        addGroup(GROUP_MEMORY_DEVICES, kMemoryDeviceIcon, tr("Memory Devices"),
                 memory_devices);
    }

    QStringList memory_errors;
    for (int i = 0; i < dmi_->memory_error_size(); ++i)
        memory_errors << memoryErrorTitle(i);

    if (!memory_errors.isEmpty())
        addGroup(GROUP_MEMORY_ERRORS, kMemoryErrorIcon, tr("Memory Errors"), memory_errors);

    QStringList memory_array_addresses;
    for (int i = 0; i < dmi_->memory_array_address_size(); ++i)
        memory_array_addresses << memoryArrayAddressTitle(i);

    if (!memory_array_addresses.isEmpty())
    {
        addGroup(GROUP_MEMORY_ARRAY_ADDRESSES, kMemoryArrayAddressIcon,
                 tr("Memory Array Addresses"), memory_array_addresses);
    }

    QStringList memory_device_addresses;
    for (int i = 0; i < dmi_->memory_device_address_size(); ++i)
        memory_device_addresses << memoryDeviceAddressTitle(i);

    if (!memory_device_addresses.isEmpty())
    {
        addGroup(GROUP_MEMORY_DEVICE_ADDRESSES, kMemoryDeviceAddressIcon,
                 tr("Memory Device Addresses"), memory_device_addresses);
    }

    if (!dmi_->boot_status().empty())
        addGroup(GROUP_SYSTEM_BOOT, kSystemBootIcon, tr("System Boot"), { tr("System Boot") });

    QStringList tpm_devices;
    for (int i = 0; i < dmi_->tpm_device_size(); ++i)
        tpm_devices << tpmDeviceTitle(i);

    if (!tpm_devices.isEmpty())
        addGroup(GROUP_TPM_DEVICES, kTpmDeviceIcon, tr("TPM Device"), tpm_devices);

    if (dmi_->has_misc())
        addGroup(GROUP_MISC, kMiscIcon, tr("Misc"), { tr("DMI Properties") });

    if (!isStateRestored())
        ui->tree->resizeColumnToContents(0);

    // Selecting the first table fills the lower pane.
    if (ui->tree->topLevelItemCount())
        ui->tree->setCurrentItem(ui->tree->topLevelItem(0));
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetDmi::treeWidget()
{
    return ui->tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::onContextMenu(const QPoint& point)
{
    showContextMenu(ui->tree, point);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::onParametersContextMenu(const QPoint& point)
{
    showContextMenu(ui->tree_params, point);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::onCurrentTableChanged()
{
    ui->tree_params->clear();

    QTreeWidgetItem* current = ui->tree->currentItem();
    if (!current || !dmi_)
        return;

    const int group = current->data(0, kGroupRole).toInt();
    const int index = current->data(0, kIndexRole).toInt();

    QList<QTreeWidgetItem*> items;

    // The item of a group shows every entry it holds, the item of an entry only the parameters of
    // that entry.
    switch (group)
    {
        case GROUP_BIOS:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->bios_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->bios_size(); ++i)
                items << new Item(kBiosIcon, biosTitle(i), biosParameters(i));
        }
        break;

        case GROUP_BASEBOARD:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->baseboard_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->baseboard_size(); ++i)
                items << new Item(kBaseboardIcon, baseboardTitle(i), baseboardParameters(i));
        }
        break;

        case GROUP_CHASSIS:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->chassis_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->chassis_size(); ++i)
                items << new Item(kChassisIcon, chassisTitle(i), chassisParameters(i));
        }
        break;

        case GROUP_PROCESSORS:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->processor_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->processor_size(); ++i)
                items << new Item(kProcessorIcon, processorTitle(i), processorParameters(i));
        }
        break;

        case GROUP_CACHES:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->cache_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->cache_size(); ++i)
                items << new Item(kCacheIcon, cacheTitle(i), cacheParameters(i));
        }
        break;

        case GROUP_PORT_CONNECTORS:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->port_connector_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->port_connector_size(); ++i)
            {
                items << new Item(kPortConnectorIcon, portConnectorTitle(i),
                                  portConnectorParameters(i));
            }
        }
        break;

        case GROUP_SYSTEM_SLOTS:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->system_slot_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->system_slot_size(); ++i)
                items << new Item(kSystemSlotIcon, systemSlotTitle(i), systemSlotParameters(i));
        }
        break;

        case GROUP_ON_BOARD_DEVICES:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->on_board_device_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->on_board_device_size(); ++i)
            {
                items << new Item(kOnBoardDeviceIcon, onBoardDeviceTitle(i),
                                  onBoardDeviceParameters(i));
            }
        }
        break;

        case GROUP_MEMORY_ARRAYS:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->memory_array_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->memory_array_size(); ++i)
            {
                items << new Item(kMemoryArrayIcon, memoryArrayTitle(i),
                                  memoryArrayParameters(i));
            }
        }
        break;

        case GROUP_MEMORY_DEVICES:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->memory_device_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->memory_device_size(); ++i)
            {
                items << new Item(kMemoryDeviceIcon, memoryDeviceTitle(i),
                                  memoryDeviceParameters(i));
            }
        }
        break;

        case GROUP_TPM_DEVICES:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->tpm_device_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->tpm_device_size(); ++i)
                items << new Item(kTpmDeviceIcon, tpmDeviceTitle(i), tpmDeviceParameters(i));
        }
        break;

        case GROUP_OEM_STRINGS:
            items << new Item(kOemStringsIcon, tr("OEM Strings"), oemStringsParameters());
            break;

        case GROUP_CONFIGURATION_OPTIONS:
        {
            items << new Item(kConfigurationOptionIcon, tr("Configuration Options"),
                              configurationOptionParameters());
        }
        break;

        case GROUP_MEMORY_ERRORS:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->memory_error_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->memory_error_size(); ++i)
                items << new Item(kMemoryErrorIcon, memoryErrorTitle(i), memoryErrorParameters(i));
        }
        break;

        case GROUP_MEMORY_ARRAY_ADDRESSES:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->memory_array_address_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->memory_array_address_size(); ++i)
            {
                items << new Item(kMemoryArrayAddressIcon, memoryArrayAddressTitle(i),
                                  memoryArrayAddressParameters(i));
            }
        }
        break;

        case GROUP_MEMORY_DEVICE_ADDRESSES:
        {
            const int first = index < 0 ? 0 : index;
            const int last = index < 0 ? dmi_->memory_device_address_size() - 1 : index;

            for (int i = first; i <= last && i < dmi_->memory_device_address_size(); ++i)
            {
                items << new Item(kMemoryDeviceAddressIcon, memoryDeviceAddressTitle(i),
                                  memoryDeviceAddressParameters(i));
            }
        }
        break;

        case GROUP_SYSTEM_BOOT:
            items << new Item(kSystemBootIcon, tr("System Boot"), systemBootParameters());
            break;

        case GROUP_MISC:
            items << new Item(kMiscIcon, tr("DMI Properties"), miscParameters());
            break;

        default:
            break;
    }

    ui->tree_params->addTopLevelItems(items);

    // Groups of parameters are nested, and there is nothing to hide behind a collapsed item.
    ui->tree_params->expandAll();

    for (int i = 0; i < ui->tree_params->columnCount(); ++i)
        ui->tree_params->resizeColumnToContents(i);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::addGroup(int group, const QString& icon_path, const QString& title,
                                const QStringList& entries)
{
    QList<QTreeWidgetItem*> childs;

    // A group of a single table has nothing to choose from and stays a leaf.
    if (entries.size() > 1)
    {
        for (int i = 0; i < entries.size(); ++i)
        {
            QTreeWidgetItem* child = new QTreeWidgetItem();

            child->setText(0, entries[i]);
            child->setData(0, kGroupRole, group);
            child->setData(0, kIndexRole, i);

            childs << child;
        }
    }

    QTreeWidgetItem* item = new Item(icon_path, title, childs);
    item->setData(0, kGroupRole, group);
    item->setData(0, kIndexRole, -1);

    // The space of the branch indicator is only worth taking when there is something to expand.
    if (!childs.isEmpty())
        ui->tree->setRootIsDecorated(true);

    // The group stays collapsed: the list of tables is a table of contents, and its entries are
    // only needed when the user goes after one of them.
    ui->tree->addTopLevelItem(item);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetDmi::showContextMenu(QTreeWidget* tree, const QPoint& point)
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
QString SysInfoWidgetDmi::biosTitle(int index) const
{
    const proto::system_info::Dmi::Bios& bios = dmi_->bios(index);

    if (!bios.vendor().empty())
        return QString::fromStdString(bios.vendor());

    return tr("BIOS");
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::biosParameters(int index) const
{
    const proto::system_info::Dmi::Bios& bios = dmi_->bios(index);
    QList<QTreeWidgetItem*> items;

    if (!bios.vendor().empty())
        items << mk(tr("Vendor"), QString::fromStdString(bios.vendor()));

    if (!bios.version().empty())
        items << mk(tr("Version"), QString::fromStdString(bios.version()));

    if (!bios.release_date().empty())
        items << mk(tr("Release Date"), QString::fromStdString(bios.release_date()));

    if (bios.address())
    {
        items << mk(tr("Address"),
                    QString("0x%1").arg(QString::number(bios.address(), 16).toUpper()));
    }

    if (bios.rom_size())
        items << mk(tr("ROM Size"), Formatter::sizeToString(bios.rom_size()));

    if (!bios.revision().empty())
        items << mk(tr("Revision"), QString::fromStdString(bios.revision()));

    if (!bios.firmware_revision().empty())
    {
        items << mk(tr("Firmware Revision"),
                    QString::fromStdString(bios.firmware_revision()));
    }

    QList<QTreeWidgetItem*> characteristics;

    for (int i = 0; i < bios.characteristic_size(); ++i)
        characteristics << mk(QString::fromStdString(bios.characteristic(i)), QString());

    if (!characteristics.isEmpty())
        items << new Item(tr("Characteristics"), characteristics);

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::baseboardTitle(int index) const
{
    const proto::system_info::Dmi::Baseboard& baseboard = dmi_->baseboard(index);

    if (!baseboard.product().empty())
        return QString::fromStdString(baseboard.product());

    if (!baseboard.manufacturer().empty())
        return QString::fromStdString(baseboard.manufacturer());

    return tr("Board %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::baseboardParameters(int index) const
{
    const proto::system_info::Dmi::Baseboard& baseboard = dmi_->baseboard(index);
    QList<QTreeWidgetItem*> items;

    if (!baseboard.manufacturer().empty())
        items << mk(tr("Manufacturer"), QString::fromStdString(baseboard.manufacturer()));

    if (!baseboard.product().empty())
        items << mk(tr("Product"), QString::fromStdString(baseboard.product()));

    if (!baseboard.version().empty())
        items << mk(tr("Version"), QString::fromStdString(baseboard.version()));

    if (!baseboard.serial_number().empty())
        items << mk(tr("Serial Number"), QString::fromStdString(baseboard.serial_number()));

    if (!baseboard.asset_tag().empty())
        items << mk(tr("Asset Tag"), QString::fromStdString(baseboard.asset_tag()));

    if (!baseboard.location().empty())
        items << mk(tr("Location in Chassis"), QString::fromStdString(baseboard.location()));

    if (!baseboard.type().empty())
        items << mk(tr("Type"), QString::fromStdString(baseboard.type()));

    QList<QTreeWidgetItem*> features;

    features << mk(tr("Hosting Board"), baseboard.hosting_board() ? tr("Yes") : tr("No"));
    features << mk(tr("Requires Daughter Board"),
                   baseboard.requires_daughter_board() ? tr("Yes") : tr("No"));
    features << mk(tr("Removable"), baseboard.removable() ? tr("Yes") : tr("No"));
    features << mk(tr("Replaceable"), baseboard.replaceable() ? tr("Yes") : tr("No"));
    features << mk(tr("Hot Swappable"), baseboard.hot_swappable() ? tr("Yes") : tr("No"));

    items << new Item(tr("Features"), features);

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::chassisTitle(int index) const
{
    const proto::system_info::Dmi::Chassis& chassis = dmi_->chassis(index);

    if (!chassis.type().empty())
        return QString::fromStdString(chassis.type());

    if (!chassis.manufacturer().empty())
        return QString::fromStdString(chassis.manufacturer());

    return tr("Chassis %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::chassisParameters(int index) const
{
    const proto::system_info::Dmi::Chassis& chassis = dmi_->chassis(index);
    QList<QTreeWidgetItem*> items;

    if (!chassis.manufacturer().empty())
        items << mk(tr("Manufacturer"), QString::fromStdString(chassis.manufacturer()));

    if (!chassis.type().empty())
        items << mk(tr("Type"), QString::fromStdString(chassis.type()));

    items << mk(tr("Lock Present"), chassis.lock_present() ? tr("Yes") : tr("No"));

    if (!chassis.version().empty())
        items << mk(tr("Version"), QString::fromStdString(chassis.version()));

    if (!chassis.serial_number().empty())
        items << mk(tr("Serial Number"), QString::fromStdString(chassis.serial_number()));

    if (!chassis.asset_tag().empty())
        items << mk(tr("Asset Tag"), QString::fromStdString(chassis.asset_tag()));

    if (!chassis.sku_number().empty())
        items << mk(tr("SKU Number"), QString::fromStdString(chassis.sku_number()));

    if (!chassis.boot_up_state().empty())
        items << mk(tr("Boot-up State"), QString::fromStdString(chassis.boot_up_state()));

    if (!chassis.power_supply_state().empty())
    {
        items << mk(tr("Power Supply State"),
                    QString::fromStdString(chassis.power_supply_state()));
    }

    if (!chassis.thermal_state().empty())
        items << mk(tr("Thermal State"), QString::fromStdString(chassis.thermal_state()));

    if (!chassis.security_status().empty())
        items << mk(tr("Security Status"), QString::fromStdString(chassis.security_status()));

    // Both the height and the number of power cords are unspecified when zero.
    if (chassis.height())
        items << mk(tr("Height"), tr("%1 U").arg(chassis.height()));

    if (chassis.power_cords())
        items << mk(tr("Power Cords"), QString::number(chassis.power_cords()));

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::processorTitle(int index) const
{
    const proto::system_info::Dmi::Processor& processor = dmi_->processor(index);

    if (!processor.version().empty())
        return QString::fromStdString(processor.version());

    if (!processor.socket_designation().empty())
        return QString::fromStdString(processor.socket_designation());

    return tr("Processor %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::processorParameters(int index) const
{
    const proto::system_info::Dmi::Processor& processor = dmi_->processor(index);
    QList<QTreeWidgetItem*> items;

    if (!processor.populated())
    {
        items << mk(tr("Installed"), tr("No"));
        return items;
    }

    if (!processor.manufacturer().empty())
        items << mk(tr("Manufacturer"), QString::fromStdString(processor.manufacturer()));

    if (!processor.version().empty())
        items << mk(tr("Version"), QString::fromStdString(processor.version()));

    if (!processor.family().empty())
        items << mk(tr("Family"), QString::fromStdString(processor.family()));

    if (!processor.type().empty())
        items << mk(tr("Type"), QString::fromStdString(processor.type()));

    if (!processor.status().empty())
        items << mk(tr("Status"), QString::fromStdString(processor.status()));

    if (!processor.socket_designation().empty())
    {
        items << mk(tr("Socket Designation"),
                    QString::fromStdString(processor.socket_designation()));
    }

    if (!processor.socket().empty())
        items << mk(tr("Socket"), QString::fromStdString(processor.socket()));

    if (!processor.socket_type().empty())
        items << mk(tr("Socket Type"), QString::fromStdString(processor.socket_type()));

    if (processor.id())
        items << mk(tr("ID"), QString("%1").arg(processor.id(), 16, 16, QChar('0')).toUpper());

    if (processor.voltage() > 0.0)
        items << mk(tr("Voltage"), tr("%1 V").arg(processor.voltage(), 0, 'f', 1));

    if (processor.external_clock())
        items << mk(tr("External Clock"), tr("%1 MHz").arg(processor.external_clock()));

    if (processor.max_speed())
        items << mk(tr("Max Speed"), tr("%1 MHz").arg(processor.max_speed()));

    if (processor.current_speed())
        items << mk(tr("Current Speed"), tr("%1 MHz").arg(processor.current_speed()));

    if (processor.core_count())
        items << mk(tr("Core Count"), QString::number(processor.core_count()));

    if (processor.core_enabled())
        items << mk(tr("Cores Enabled"), QString::number(processor.core_enabled()));

    if (processor.thread_count())
        items << mk(tr("Thread Count"), QString::number(processor.thread_count()));

    if (processor.thread_enabled())
        items << mk(tr("Threads Enabled"), QString::number(processor.thread_enabled()));

    if (!processor.serial_number().empty())
        items << mk(tr("Serial Number"), QString::fromStdString(processor.serial_number()));

    if (!processor.asset_tag().empty())
        items << mk(tr("Asset Tag"), QString::fromStdString(processor.asset_tag()));

    if (!processor.part_number().empty())
        items << mk(tr("Part Number"), QString::fromStdString(processor.part_number()));

    items << mk(tr("64-bit Capable"), processor.support_64bit() ? tr("Yes") : tr("No"));
    items << mk(tr("Multi-Core"), processor.support_multi_core() ? tr("Yes") : tr("No"));
    items << mk(tr("Hardware Thread"), processor.support_hardware_thread() ? tr("Yes") : tr("No"));

    items << mk(tr("Execute Protection"),
                processor.support_execute_protection() ? tr("Yes") : tr("No"));
    items << mk(tr("Enhanced Virtualization"),
                processor.support_enhanced_virtualization() ? tr("Yes") : tr("No"));
    items << mk(tr("Power/Performance Control"),
                processor.support_power_control() ? tr("Yes") : tr("No"));

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::cacheTitle(int index) const
{
    const proto::system_info::Dmi::Cache& cache = dmi_->cache(index);

    if (!cache.designation().empty())
        return QString::fromStdString(cache.designation());

    return tr("L%1 Cache").arg(cache.level());
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::cacheParameters(int index) const
{
    const proto::system_info::Dmi::Cache& cache = dmi_->cache(index);
    QList<QTreeWidgetItem*> items;

    if (!cache.designation().empty())
        items << mk(tr("Socket Designation"), QString::fromStdString(cache.designation()));

    items << mk(tr("Level"), QString::number(cache.level()));

    if (!cache.type().empty())
        items << mk(tr("Type"), QString::fromStdString(cache.type()));

    if (cache.current_size())
        items << mk(tr("Installed Size"), Formatter::sizeToString(cache.current_size()));

    if (cache.max_size())
        items << mk(tr("Maximum Size"), Formatter::sizeToString(cache.max_size()));

    if (!cache.location().empty())
        items << mk(tr("Location"), QString::fromStdString(cache.location()));

    if (!cache.mode().empty())
        items << mk(tr("Operational Mode"), QString::fromStdString(cache.mode()));

    if (!cache.sram_type().empty())
        items << mk(tr("SRAM Type"), QString::fromStdString(cache.sram_type()));

    if (!cache.supported_sram_type().empty())
    {
        items << mk(tr("Supported SRAM Types"),
                    QString::fromStdString(cache.supported_sram_type()));
    }

    if (!cache.error_correction_type().empty())
    {
        items << mk(tr("Error Correction Type"),
                    QString::fromStdString(cache.error_correction_type()));
    }

    if (!cache.associativity().empty())
        items << mk(tr("Associativity"), QString::fromStdString(cache.associativity()));

    // Zero means the speed is unknown.
    if (cache.speed())
        items << mk(tr("Speed"), tr("%1 ns").arg(cache.speed()));

    items << mk(tr("Enabled"), cache.enabled() ? tr("Yes") : tr("No"));
    items << mk(tr("Socketed"), cache.socketed() ? tr("Yes") : tr("No"));

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::portConnectorTitle(int index) const
{
    const proto::system_info::Dmi::PortConnector& port = dmi_->port_connector(index);

    if (!port.external_designator().empty())
        return QString::fromStdString(port.external_designator());

    if (!port.internal_designator().empty())
        return QString::fromStdString(port.internal_designator());

    if (!port.type().empty())
        return QString::fromStdString(port.type());

    return tr("Port %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::portConnectorParameters(int index) const
{
    const proto::system_info::Dmi::PortConnector& port = dmi_->port_connector(index);
    QList<QTreeWidgetItem*> items;

    if (!port.type().empty())
        items << mk(tr("Port Type"), QString::fromStdString(port.type()));

    if (!port.internal_designator().empty())
    {
        items << mk(tr("Internal Designator"),
                    QString::fromStdString(port.internal_designator()));
    }

    if (!port.internal_connector_type().empty())
    {
        items << mk(tr("Internal Connector Type"),
                    QString::fromStdString(port.internal_connector_type()));
    }

    if (!port.external_designator().empty())
    {
        items << mk(tr("External Designator"),
                    QString::fromStdString(port.external_designator()));
    }

    if (!port.external_connector_type().empty())
    {
        items << mk(tr("External Connector Type"),
                    QString::fromStdString(port.external_connector_type()));
    }

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::systemSlotTitle(int index) const
{
    const proto::system_info::Dmi::SystemSlot& slot = dmi_->system_slot(index);

    if (!slot.designation().empty())
        return QString::fromStdString(slot.designation());

    if (!slot.type().empty())
        return QString::fromStdString(slot.type());

    return tr("Slot %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::systemSlotParameters(int index) const
{
    const proto::system_info::Dmi::SystemSlot& slot = dmi_->system_slot(index);
    QList<QTreeWidgetItem*> items;

    if (!slot.designation().empty())
        items << mk(tr("Designation"), QString::fromStdString(slot.designation()));

    if (!slot.type().empty())
        items << mk(tr("Type"), QString::fromStdString(slot.type()));

    if (!slot.data_bus_width().empty())
        items << mk(tr("Data Bus Width"), QString::fromStdString(slot.data_bus_width()));

    if (!slot.usage().empty())
        items << mk(tr("Current Usage"), QString::fromStdString(slot.usage()));

    if (!slot.length().empty())
        items << mk(tr("Length"), QString::fromStdString(slot.length()));

    if (!slot.bus_address().empty())
        items << mk(tr("Bus Address"), QString::fromStdString(slot.bus_address()));

    items << mk(tr("ID"), QString::number(slot.id()));

    QList<QTreeWidgetItem*> characteristics;

    characteristics << mk(tr("5 V Provided"), slot.provides_5_volts() ? tr("Yes") : tr("No"));
    characteristics << mk(tr("3.3 V Provided"), slot.provides_3_volts() ? tr("Yes") : tr("No"));
    characteristics << mk(tr("Shared"), slot.shared() ? tr("Yes") : tr("No"));
    characteristics << mk(tr("PME Signal"), slot.supports_pme() ? tr("Yes") : tr("No"));
    characteristics << mk(tr("Hot Plug"), slot.supports_hot_plug() ? tr("Yes") : tr("No"));
    characteristics << mk(tr("SMBus Signal"), slot.supports_smbus() ? tr("Yes") : tr("No"));
    characteristics << mk(tr("Bifurcation"), slot.supports_bifurcation() ? tr("Yes") : tr("No"));

    items << new Item(tr("Characteristics"), characteristics);

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::onBoardDeviceTitle(int index) const
{
    const proto::system_info::Dmi::OnBoardDevice& device = dmi_->on_board_device(index);

    if (!device.description().empty())
        return QString::fromStdString(device.description());

    if (!device.type().empty())
        return QString::fromStdString(device.type());

    return tr("Device %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::onBoardDeviceParameters(int index) const
{
    const proto::system_info::Dmi::OnBoardDevice& device = dmi_->on_board_device(index);
    QList<QTreeWidgetItem*> items;

    if (!device.description().empty())
        items << mk(tr("Description"), QString::fromStdString(device.description()));

    if (!device.type().empty())
        items << mk(tr("Type"), QString::fromStdString(device.type()));

    // The legacy table reports no instance, which starts at one in the extended one.
    if (device.instance())
        items << mk(tr("Type Instance"), QString::number(device.instance()));

    if (!device.bus_address().empty())
        items << mk(tr("Bus Address"), QString::fromStdString(device.bus_address()));

    items << mk(tr("Enabled"), device.enabled() ? tr("Yes") : tr("No"));

    return items;
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::oemStringsParameters() const
{
    QList<QTreeWidgetItem*> items;

    for (int i = 0; i < dmi_->oem_string_size(); ++i)
    {
        items << mk(tr("String %1").arg(i + 1),
                    QString::fromStdString(dmi_->oem_string(i)));
    }

    return items;
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::configurationOptionParameters() const
{
    QList<QTreeWidgetItem*> items;

    for (int i = 0; i < dmi_->configuration_option_size(); ++i)
    {
        items << mk(tr("Option %1").arg(i + 1),
                    QString::fromStdString(dmi_->configuration_option(i)));
    }

    return items;
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::systemBootParameters() const
{
    QList<QTreeWidgetItem*> items;

    items << mk(tr("Boot Status"), QString::fromStdString(dmi_->boot_status()));

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::memoryArrayTitle(int index) const
{
    const proto::system_info::Dmi::MemoryArray& memory_array = dmi_->memory_array(index);

    if (!memory_array.use().empty())
        return QString::fromStdString(memory_array.use());

    if (!memory_array.location().empty())
        return QString::fromStdString(memory_array.location());

    return tr("Array %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::memoryArrayParameters(int index) const
{
    const proto::system_info::Dmi::MemoryArray& memory_array = dmi_->memory_array(index);
    QList<QTreeWidgetItem*> items;

    if (!memory_array.location().empty())
        items << mk(tr("Location"), QString::fromStdString(memory_array.location()));

    if (!memory_array.use().empty())
        items << mk(tr("Use"), QString::fromStdString(memory_array.use()));

    if (!memory_array.error_correction().empty())
    {
        items << mk(tr("Error Correction"),
                    QString::fromStdString(memory_array.error_correction()));
    }

    if (memory_array.max_capacity())
    {
        items << mk(tr("Maximum Capacity"),
                    Formatter::sizeToString(memory_array.max_capacity()));
    }

    items << mk(tr("Number of Devices"), QString::number(memory_array.device_count()));

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::memoryDeviceTitle(int index) const
{
    const proto::system_info::Dmi::MemoryDevice& memory_device = dmi_->memory_device(index);

    if (!memory_device.location().empty())
        return QString::fromStdString(memory_device.location());

    if (!memory_device.bank().empty())
        return QString::fromStdString(memory_device.bank());

    return tr("Device %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::memoryDeviceParameters(int index) const
{
    const proto::system_info::Dmi::MemoryDevice& memory_device = dmi_->memory_device(index);
    QList<QTreeWidgetItem*> items;

    if (!memory_device.location().empty())
        items << mk(tr("Location"), QString::fromStdString(memory_device.location()));

    if (!memory_device.bank().empty())
        items << mk(tr("Bank"), QString::fromStdString(memory_device.bank()));

    // An empty slot has nothing but its location to show.
    if (!memory_device.present())
    {
        items << mk(tr("Installed"), tr("No"));
        return items;
    }

    if (!memory_device.manufacturer().empty())
        items << mk(tr("Manufacturer"), QString::fromStdString(memory_device.manufacturer()));

    if (memory_device.size())
        items << mk(tr("Size"), Formatter::sizeToString(memory_device.size()));

    if (!memory_device.type().empty())
        items << mk(tr("Type"), QString::fromStdString(memory_device.type()));

    if (!memory_device.type_detail().empty())
        items << mk(tr("Type Detail"), QString::fromStdString(memory_device.type_detail()));

    if (!memory_device.form_factor().empty())
        items << mk(tr("Form Factor"), QString::fromStdString(memory_device.form_factor()));

    if (!memory_device.technology().empty())
        items << mk(tr("Technology"), QString::fromStdString(memory_device.technology()));

    if (memory_device.speed())
        items << mk(tr("Speed"), tr("%1 MT/s").arg(memory_device.speed()));

    if (memory_device.configured_speed())
    {
        items << mk(tr("Configured Speed"),
                    tr("%1 MT/s").arg(memory_device.configured_speed()));
    }

    if (memory_device.total_width())
        items << mk(tr("Total Width"), tr("%1 bit").arg(memory_device.total_width()));

    if (memory_device.data_width())
        items << mk(tr("Data Width"), tr("%1 bit").arg(memory_device.data_width()));

    if (memory_device.rank())
        items << mk(tr("Rank"), QString::number(memory_device.rank()));

    if (memory_device.min_voltage())
        items << mk(tr("Minimum Voltage"), tr("%1 mV").arg(memory_device.min_voltage()));

    if (memory_device.max_voltage())
        items << mk(tr("Maximum Voltage"), tr("%1 mV").arg(memory_device.max_voltage()));

    if (memory_device.configured_voltage())
    {
        items << mk(tr("Configured Voltage"),
                    tr("%1 mV").arg(memory_device.configured_voltage()));
    }

    if (!memory_device.part_number().empty())
        items << mk(tr("Part Number"), QString::fromStdString(memory_device.part_number()));

    if (!memory_device.serial_number().empty())
        items << mk(tr("Serial Number"), QString::fromStdString(memory_device.serial_number()));

    if (!memory_device.asset_tag().empty())
        items << mk(tr("Asset Tag"), QString::fromStdString(memory_device.asset_tag()));

    if (!memory_device.firmware_version().empty())
    {
        items << mk(tr("Firmware Version"),
                    QString::fromStdString(memory_device.firmware_version()));
    }

    if (memory_device.non_volatile_size())
    {
        items << mk(tr("Non-volatile Size"),
                    Formatter::sizeToString(memory_device.non_volatile_size()));
    }

    if (memory_device.volatile_size())
    {
        items << mk(tr("Volatile Size"),
                    Formatter::sizeToString(memory_device.volatile_size()));
    }

    if (memory_device.cache_size())
        items << mk(tr("Cache Size"), Formatter::sizeToString(memory_device.cache_size()));

    if (memory_device.logical_size())
        items << mk(tr("Logical Size"), Formatter::sizeToString(memory_device.logical_size()));

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::memoryErrorTitle(int index) const
{
    // Nothing but the position tells the records apart: the tables that point at them are the ones
    // that give them a meaning.
    return tr("Record %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::memoryErrorParameters(int index) const
{
    const proto::system_info::Dmi::MemoryError& error = dmi_->memory_error(index);
    QList<QTreeWidgetItem*> items;

    if (!error.type().empty())
        items << mk(tr("Type"), QString::fromStdString(error.type()));

    if (!error.granularity().empty())
        items << mk(tr("Granularity"), QString::fromStdString(error.granularity()));

    if (!error.operation().empty())
        items << mk(tr("Operation"), QString::fromStdString(error.operation()));

    // Everything below is left out by the firmware unless it has an error to report.
    if (error.syndrome())
    {
        items << mk(tr("Vendor Syndrome"),
                    QString("0x%1").arg(QString::number(error.syndrome(), 16).toUpper()));
    }

    if (error.array_address())
    {
        items << mk(tr("Memory Array Address"),
                    QString("0x%1").arg(QString::number(error.array_address(), 16).toUpper()));
    }

    if (error.device_address())
    {
        items << mk(tr("Device Address"),
                    QString("0x%1").arg(QString::number(error.device_address(), 16).toUpper()));
    }

    if (error.resolution())
        items << mk(tr("Resolution"), tr("%1 bytes").arg(error.resolution()));

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::memoryArrayAddressTitle(int index) const
{
    const proto::system_info::Dmi::MemoryArrayAddress& address = dmi_->memory_array_address(index);

    if (!address.array().empty())
        return QString::fromStdString(address.array());

    return tr("Range %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::memoryArrayAddressParameters(int index) const
{
    const proto::system_info::Dmi::MemoryArrayAddress& address = dmi_->memory_array_address(index);
    QList<QTreeWidgetItem*> items;

    if (!address.array().empty())
        items << mk(tr("Array"), QString::fromStdString(address.array()));

    // A range of no size means the firmware left the addresses out.
    if (address.size())
    {
        items << mk(tr("Starting Address"),
                    QString("0x%1").arg(QString::number(address.start_address(), 16).toUpper()));
        items << mk(tr("Ending Address"),
                    QString("0x%1").arg(QString::number(address.end_address(), 16).toUpper()));
        items << mk(tr("Size"), Formatter::sizeToString(address.size()));
    }

    // The number of devices forming a single row, zero when the firmware does not report it.
    if (address.partition_width())
        items << mk(tr("Partition Width"), QString::number(address.partition_width()));

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::memoryDeviceAddressTitle(int index) const
{
    const proto::system_info::Dmi::MemoryDeviceAddress& address =
        dmi_->memory_device_address(index);

    if (!address.device().empty())
        return QString::fromStdString(address.device());

    return tr("Range %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::memoryDeviceAddressParameters(int index) const
{
    const proto::system_info::Dmi::MemoryDeviceAddress& address =
        dmi_->memory_device_address(index);
    QList<QTreeWidgetItem*> items;

    if (!address.device().empty())
        items << mk(tr("Device"), QString::fromStdString(address.device()));

    // A range of no size means the firmware left the addresses out.
    if (address.size())
    {
        items << mk(tr("Starting Address"),
                    QString("0x%1").arg(QString::number(address.start_address(), 16).toUpper()));
        items << mk(tr("Ending Address"),
                    QString("0x%1").arg(QString::number(address.end_address(), 16).toUpper()));
        items << mk(tr("Size"), Formatter::sizeToString(address.size()));
    }

    if (address.row_position())
        items << mk(tr("Partition Row Position"), QString::number(address.row_position()));

    if (address.interleave_position())
    {
        items << mk(tr("Interleave Position"),
                    QString::number(address.interleave_position()));
    }

    if (address.interleave_depth())
    {
        items << mk(tr("Interleaved Data Depth"),
                    QString::number(address.interleave_depth()));
    }

    return items;
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidgetDmi::tpmDeviceTitle(int index) const
{
    const proto::system_info::Dmi::TpmDevice& tpm = dmi_->tpm_device(index);

    if (!tpm.description().empty())
        return QString::fromStdString(tpm.description());

    if (!tpm.vendor_id().empty())
        return QString::fromStdString(tpm.vendor_id());

    return tr("Device %1").arg(index + 1);
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::tpmDeviceParameters(int index) const
{
    const proto::system_info::Dmi::TpmDevice& tpm = dmi_->tpm_device(index);
    QList<QTreeWidgetItem*> items;

    if (!tpm.description().empty())
        items << mk(tr("Description"), QString::fromStdString(tpm.description()));

    if (!tpm.vendor_id().empty())
        items << mk(tr("Vendor ID"), QString::fromStdString(tpm.vendor_id()));

    if (!tpm.spec_version().empty())
        items << mk(tr("Specification Version"), QString::fromStdString(tpm.spec_version()));

    if (!tpm.firmware_version().empty())
        items << mk(tr("Firmware Version"), QString::fromStdString(tpm.firmware_version()));

    QList<QTreeWidgetItem*> characteristics;

    characteristics << mk(tr("Configurable by Firmware"),
                          tpm.configurable_by_firmware() ? tr("Yes") : tr("No"));
    characteristics << mk(tr("Configurable by Software"),
                          tpm.configurable_by_software() ? tr("Yes") : tr("No"));
    characteristics << mk(tr("Configurable by OEM"),
                          tpm.configurable_by_oem() ? tr("Yes") : tr("No"));

    items << new Item(tr("Characteristics"), characteristics);

    return items;
}

//--------------------------------------------------------------------------------------------------
QList<QTreeWidgetItem*> SysInfoWidgetDmi::miscParameters() const
{
    const proto::system_info::Dmi::Misc& misc = dmi_->misc();
    QList<QTreeWidgetItem*> items;

    if (!misc.smbios_version().empty())
        items << mk(tr("SMBIOS Version"), QString::fromStdString(misc.smbios_version()));

    if (misc.structure_count())
        items << mk(tr("Structures"), QString::number(misc.structure_count()));

    if (misc.structure_size())
        items << mk(tr("Structures Size"), tr("%1 bytes").arg(misc.structure_size()));

    return items;
}
