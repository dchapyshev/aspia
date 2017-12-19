//
// PROJECT:         Aspia
// FILE:            protocol/category_group_hardware.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/printer_enumerator.h"
#include "base/devices/battery_enumerator.h"
#include "base/devices/monitor_enumerator.h"
#include "base/devices/video_adapter_enumarator.h"
#include "base/devices/physical_drive_enumerator.h"
#include "base/files/logical_drive_enumerator.h"
#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/bitset.h"
#include "base/cpu_info.h"
#include "protocol/category_group_hardware.h"
#include "ui/system_info/output.h"
#include "ui/resource.h"

namespace aspia {

//
// CategoryDmiBios
//

const char* CategoryDmiBios::Name() const
{
    return "BIOS";
}

Category::IconId CategoryDmiBios::Icon() const
{
    return IDI_BIOS;
}

const char* CategoryDmiBios::Guid() const
{
    return "{B0B73D57-2CDC-4814-9AE0-C7AF7DDDD60E}";
}

void CategoryDmiBios::Parse(Table& table, const std::string& data)
{
    proto::DmiBios message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    if (!message.manufacturer().empty())
        table.AddParam("Manufacturer", Value::String(message.manufacturer()));

    if (!message.version().empty())
        table.AddParam("Version", Value::String(message.version()));

    if (!message.date().empty())
        table.AddParam("Date", Value::String(message.date()));

    if (message.size() != 0)
        table.AddParam("Size", Value::Number(message.size(), "kB"));

    if (!message.bios_revision().empty())
        table.AddParam("BIOS Revision", Value::String(message.bios_revision()));

    if (!message.firmware_revision().empty())
        table.AddParam("Firmware Revision", Value::String(message.firmware_revision()));

    if (!message.address().empty())
        table.AddParam("Address", Value::String(message.address()));

    if (message.runtime_size() != 0)
        table.AddParam("Runtime Size", Value::Number(message.runtime_size(), "Bytes"));

    if (message.has_characteristics())
    {
        std::vector<std::pair<std::string, bool>> list;

        auto add = [&](const char* name, bool is_supported)
        {
            list.emplace_back(name, is_supported);
        };

        const proto::DmiBios::Characteristics& ch = message.characteristics();

        add("ISA", ch.has_isa());
        add("MCA", ch.has_mca());
        add("EISA", ch.has_eisa());
        add("PCI", ch.has_pci());
        add("PC Card (PCMCIA)", ch.has_pc_card());
        add("Plug and Play", ch.has_pnp());
        add("APM is Supported", ch.has_apm());
        add("BIOS is Upgradeable (Flash)", ch.has_bios_upgradeable());
        add("BIOS Shadowing is Allowed", ch.has_bios_shadowing());
        add("VL-VESA", ch.has_vlb());
        add("ESCD Support is Available", ch.has_escd());
        add("Boot from CD", ch.has_boot_from_cd());
        add("Selectable Boot", ch.has_selectable_boot());
        add("BIOS ROM is Socketed", ch.has_socketed_boot_rom());
        add("Boot from PC card (PCMCIA)", ch.has_boot_from_pc_card());
        add("EDD Specification is Supported", ch.has_edd());
        add("Japanese Floppy for NEC 9800 1.2 MB (int 13h)", ch.has_japanese_floppy_for_nec9800());
        add("Japanese Floppy for Toshiba 1.2 MB (int 13h)", ch.has_japanece_floppy_for_toshiba());
        add("5.25\"/360 kB Floppy (int 13h)", ch.has_525_360kb_floppy());
        add("5.25\"/1.2 MB Floppy (int 13h)", ch.has_525_12mb_floppy());
        add("3.5\"/720 kB Floppy (int 13h)", ch.has_35_720kb_floppy());
        add("3.5\"/2.88 MB Floppy (int 13h)", ch.has_35_288mb_floppy());
        add("Print Screen (int 5h)", ch.has_print_screen());
        add("8042 Keyboard (int 9h)", ch.has_8042_keyboard());
        add("Serial (int 14h)", ch.has_serial());
        add("Printer (int 17h)", ch.has_printer());
        add("CGA/Mono Video (int 10h)", ch.has_cga_video());
        add("NEC PC-98", ch.has_nec_pc98());

        add("ACPI", ch.has_acpi());
        add("USB Legacy", ch.has_usb_legacy());
        add("AGP", ch.has_agp());
        add("I2O Boot", ch.has_i2o_boot());
        add("LS-120 SuperDisk Boot", ch.has_ls120_boot());
        add("ATAPI ZIP Drive Boot", ch.has_atapi_zip_drive_boot());
        add("IEEE 1394 Boot", ch.has_ieee1394_boot());
        add("Smart Battery", ch.has_smart_battery());

        add("BIOS Boot Specification", ch.has_bios_boot_specification());
        add("Function Key-initiated Network Boot", ch.has_key_init_network_boot());
        add("Targeted Content Distribution", ch.has_targeted_content_distrib());
        add("UEFI Specification", ch.has_uefi());
        add("Virtual Machine", ch.has_virtual_machine());

        std::sort(list.begin(), list.end());

        Group group = table.AddGroup("Characteristics");

        for (const auto& list_item : list)
        {
            group.AddParam(list_item.first, Value::Bool(list_item.second));
        }
    }
}

std::string CategoryDmiBios::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    SMBios::TableEnumerator<SMBios::BiosTable> table_enumerator(*smbios);
    if (table_enumerator.IsAtEnd())
        return std::string();

    SMBios::BiosTable table = table_enumerator.GetTable();
    proto::DmiBios message;

    message.set_manufacturer(table.GetManufacturer());
    message.set_version(table.GetVersion());
    message.set_date(table.GetDate());
    message.set_size(table.GetSize());
    message.set_bios_revision(table.GetBiosRevision());
    message.set_firmware_revision(table.GetFirmwareRevision());
    message.set_address(table.GetAddress());
    message.set_runtime_size(table.GetRuntimeSize());

    proto::DmiBios::Characteristics* characteristics = message.mutable_characteristics();

    characteristics->set_has_isa(table.HasISA());
    characteristics->set_has_mca(table.HasMCA());
    characteristics->set_has_eisa(table.HasEISA());
    characteristics->set_has_pci(table.HasPCI());
    characteristics->set_has_pc_card(table.HasPCCard());
    characteristics->set_has_pnp(table.HasPNP());
    characteristics->set_has_apm(table.HasAPM());
    characteristics->set_has_bios_upgradeable(table.HasBiosUpgradeable());
    characteristics->set_has_bios_shadowing(table.HasBiosShadowing());
    characteristics->set_has_vlb(table.HasVLB());
    characteristics->set_has_escd(table.HasESCD());
    characteristics->set_has_boot_from_cd(table.HasBootFromCD());
    characteristics->set_has_selectable_boot(table.HasSelectableBoot());
    characteristics->set_has_socketed_boot_rom(table.HasSocketedBootROM());
    characteristics->set_has_boot_from_pc_card(table.HasBootFromPCCard());
    characteristics->set_has_edd(table.HasEDD());
    characteristics->set_has_japanese_floppy_for_nec9800(table.HasJapaneseFloppyForNec9800());
    characteristics->set_has_japanece_floppy_for_toshiba(table.HasJapaneceFloppyForToshiba());
    characteristics->set_has_525_360kb_floppy(table.Has525_360kbFloppy());
    characteristics->set_has_525_12mb_floppy(table.Has525_12mbFloppy());
    characteristics->set_has_35_720kb_floppy(table.Has35_720kbFloppy());
    characteristics->set_has_35_288mb_floppy(table.Has35_288mbFloppy());
    characteristics->set_has_print_screen(table.HasPrintScreen());
    characteristics->set_has_8042_keyboard(table.Has8042Keyboard());
    characteristics->set_has_serial(table.HasSerial());
    characteristics->set_has_printer(table.HasPrinter());
    characteristics->set_has_cga_video(table.HasCGAVideo());
    characteristics->set_has_nec_pc98(table.HasNecPC98());
    characteristics->set_has_acpi(table.HasACPI());
    characteristics->set_has_usb_legacy(table.HasUSBLegacy());
    characteristics->set_has_agp(table.HasAGP());
    characteristics->set_has_i2o_boot(table.HasI2OBoot());
    characteristics->set_has_ls120_boot(table.HasLS120Boot());
    characteristics->set_has_atapi_zip_drive_boot(table.HasAtapiZipDriveBoot());
    characteristics->set_has_ieee1394_boot(table.HasIeee1394Boot());
    characteristics->set_has_smart_battery(table.HasSmartBattery());
    characteristics->set_has_bios_boot_specification(table.HasBiosBootSpecification());
    characteristics->set_has_key_init_network_boot(table.HasKeyInitNetworkBoot());
    characteristics->set_has_targeted_content_distrib(table.HasTargetedContentDistrib());
    characteristics->set_has_uefi(table.HasUEFI());
    characteristics->set_has_virtual_machine(table.HasVirtualMachine());

    return message.SerializeAsString();
}

//
// CategoryDmiSystem
//

const char* CategoryDmiSystem::Name() const
{
    return "System";
}

Category::IconId CategoryDmiSystem::Icon() const
{
    return IDI_COMPUTER;
}

const char* CategoryDmiSystem::Guid() const
{
    return "{F599BBA4-AEBB-4583-A15E-9848F4C98601}";
}

void CategoryDmiSystem::Parse(Table& table, const std::string& data)
{
    proto::DmiSystem message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    if (!message.manufacturer().empty())
        table.AddParam("Manufacturer", Value::String(message.manufacturer()));

    if (!message.product_name().empty())
        table.AddParam("Product Name", Value::String(message.product_name()));

    if (!message.version().empty())
        table.AddParam("Version", Value::String(message.version()));

    if (!message.serial_number().empty())
        table.AddParam("Serial Number", Value::String(message.serial_number()));

    if (!message.uuid().empty())
        table.AddParam("UUID", Value::String(message.uuid()));

    table.AddParam("Wakeup Type", Value::String(WakeupTypeToString(message.wakeup_type())));

    if (!message.sku_number().empty())
        table.AddParam("SKU Number", Value::String(message.sku_number()));

    if (!message.family().empty())
        table.AddParam("Family", Value::String(message.family()));
}

std::string CategoryDmiSystem::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    SMBios::TableEnumerator<SMBios::SystemTable> table_enumerator(*smbios);
    if (table_enumerator.IsAtEnd())
        return std::string();

    SMBios::SystemTable table = table_enumerator.GetTable();
    proto::DmiSystem message;

    message.set_manufacturer(table.GetManufacturer());
    message.set_product_name(table.GetProductName());
    message.set_version(table.GetVersion());
    message.set_serial_number(table.GetSerialNumber());
    message.set_uuid(table.GetUUID());
    message.set_wakeup_type(table.GetWakeupType());
    message.set_sku_number(table.GetSKUNumber());
    message.set_family(table.GetFamily());

    return message.SerializeAsString();
}

// static
const char* CategoryDmiSystem::WakeupTypeToString(proto::DmiSystem::WakeupType value)
{
    switch (value)
    {
        case proto::DmiSystem::WAKEUP_TYPE_OTHER:
            return "Other";

        case proto::DmiSystem::WAKEUP_TYPE_APM_TIMER:
            return "APM Timer";

        case proto::DmiSystem::WAKEUP_TYPE_MODEM_RING:
            return "Modem Ring";

        case proto::DmiSystem::WAKEUP_TYPE_LAN_REMOTE:
            return "LAN Remote";

        case proto::DmiSystem::WAKEUP_TYPE_POWER_SWITCH:
            return "Power Switch";

        case proto::DmiSystem::WAKEUP_TYPE_PCI_PME:
            return "PCI PME#";

        case proto::DmiSystem::WAKEUP_TYPE_AC_POWER_RESTORED:
            return "AC Power Restored";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiMotherboard
//

const char* CategoryDmiBaseboard::Name() const
{
    return "Baseboard";
}

Category::IconId CategoryDmiBaseboard::Icon() const
{
    return IDI_MOTHERBOARD;
}

const char* CategoryDmiBaseboard::Guid() const
{
    return "{8143642D-3248-40F5-8FCF-629C581FFF01}";
}

void CategoryDmiBaseboard::Parse(Table& table, const std::string& data)
{
    proto::DmiBaseboard message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiBaseboard::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Baseboard #%d", index + 1));

        group.AddParam("Type", Value::String(BoardTypeToString(item.type())));

        if (!item.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufacturer()));

        if (!item.product_name().empty())
            group.AddParam("Product Name", Value::String(item.product_name()));

        if (!item.version().empty())
            group.AddParam("Version", Value::String(item.version()));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.asset_tag().empty())
            group.AddParam("Asset Tag", Value::String(item.asset_tag()));

        if (!item.location_in_chassis().empty())
            group.AddParam("Location in chassis", Value::String(item.location_in_chassis()));

        if (item.has_features())
        {
            Group features_group = group.AddGroup("Supported Features");
            const proto::DmiBaseboard::Features& features = item.features();

            features_group.AddParam("Board is a hosting board",
                                    Value::Bool(features.is_hosting_board()));
            features_group.AddParam("Board requires at least one daughter board",
                                    Value::Bool(features.is_requires_at_least_one_daughter_board()));
            features_group.AddParam("Board is removable",
                                    Value::Bool(features.is_removable()));
            features_group.AddParam("Board is replaceable",
                                    Value::Bool(features.is_replaceable()));
            features_group.AddParam("Board is hot swappable",
                                    Value::Bool(features.is_hot_swappable()));
        }
    }
}

std::string CategoryDmiBaseboard::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiBaseboard message;

    for (SMBios::TableEnumerator<SMBios::BaseboardTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::BaseboardTable table = table_enumerator.GetTable();
        proto::DmiBaseboard::Item* item = message.add_item();

        item->set_manufacturer(table.GetManufacturer());
        item->set_product_name(table.GetProductName());
        item->set_version(table.GetVersion());
        item->set_serial_number(table.GetSerialNumber());
        item->set_asset_tag(table.GetAssetTag());
        item->set_location_in_chassis(table.GetLocationInChassis());
        item->set_type(table.GetBoardType());

        proto::DmiBaseboard::Features* features = item->mutable_features();

        features->set_is_hosting_board(table.IsHostingBoard());
        features->set_is_requires_at_least_one_daughter_board(
            table.IsRequiresAtLeastOneDaughterBoard());
        features->set_is_removable(table.IsRemovable());
        features->set_is_replaceable(table.IsReplaceable());
        features->set_is_hot_swappable(table.IsHotSwappable());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiBaseboard::BoardTypeToString(proto::DmiBaseboard::BoardType type)
{
    switch (type)
    {
        case proto::DmiBaseboard::BOARD_TYPE_OTHER:
            return "Other";

        case proto::DmiBaseboard::BOARD_TYPE_SERVER_BLADE:
            return "Server Blade";

        case proto::DmiBaseboard::BOARD_TYPE_CONNECTIVITY_SWITCH:
            return "Connectivity Switch";

        case proto::DmiBaseboard::BOARD_TYPE_SYSTEM_MANAGEMENT_MODULE:
            return "System Management Module";

        case proto::DmiBaseboard::BOARD_TYPE_PROCESSOR_MODULE:
            return "Processor Module";

        case proto::DmiBaseboard::BOARD_TYPE_IO_MODULE:
            return "I/O Module";

        case proto::DmiBaseboard::BOARD_TYPE_MEMORY_MODULE:
            return "Memory Module";

        case proto::DmiBaseboard::BOARD_TYPE_DAUGHTER_BOARD:
            return "Daughter Board";

        case proto::DmiBaseboard::BOARD_TYPE_MOTHERBOARD:
            return "Motherboard";

        case proto::DmiBaseboard::BOARD_TYPE_PROCESSOR_PLUS_MEMORY_MODULE:
            return "Processor + Memory Module";

        case proto::DmiBaseboard::BOARD_TYPE_PROCESSOR_PLUS_IO_MODULE:
            return "Processor + I/O Module";

        case proto::DmiBaseboard::BOARD_TYPE_INTERCONNECT_BOARD:
            return "Interconnect Board";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiChassis
//

const char* CategoryDmiChassis::Name() const
{
    return "Chassis";
}

Category::IconId CategoryDmiChassis::Icon() const
{
    return IDI_SERVER;
}

const char* CategoryDmiChassis::Guid() const
{
    return "{81D9E51F-4A86-49FC-A37F-232D6A62EC45}";
}

void CategoryDmiChassis::Parse(Table& table, const std::string& data)
{
    proto::DmiChassis message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiChassis::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Chassis #%d", index + 1));

        if (!item.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufacturer()));

        if (!item.version().empty())
            group.AddParam("Version", Value::String(item.version()));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.asset_tag().empty())
            group.AddParam("Asset Tag", Value::String(item.asset_tag()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("OS Load Status", Value::String(StatusToString(item.os_load_status())));
        group.AddParam("Power Source Status", Value::String(StatusToString(item.power_source_status())));
        group.AddParam("Temperature Status", Value::String(StatusToString(item.temparature_status())));
        group.AddParam("Security Status", Value::String(SecurityStatusToString(item.security_status())));

        if (item.height() != 0)
            group.AddParam("Height", Value::Number(item.height(), "U"));

        if (item.number_of_power_cords() != 0)
        {
            group.AddParam("Number Of Power Cords", Value::Number(item.number_of_power_cords()));
        }
    }
}

std::string CategoryDmiChassis::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiChassis message;

    for (SMBios::TableEnumerator<SMBios::ChassisTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::ChassisTable table = table_enumerator.GetTable();
        proto::DmiChassis::Item* item = message.add_item();

        item->set_manufacturer(table.GetManufacturer());
        item->set_version(table.GetVersion());
        item->set_serial_number(table.GetSerialNumber());
        item->set_asset_tag(table.GetAssetTag());
        item->set_type(table.GetType());
        item->set_os_load_status(table.GetOSLoadStatus());
        item->set_power_source_status(table.GetPowerSourceStatus());
        item->set_temparature_status(table.GetTemperatureStatus());
        item->set_security_status(table.GetSecurityStatus());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiChassis::TypeToString(proto::DmiChassis::Type type)
{
    switch (type)
    {
        case proto::DmiChassis::TYPE_OTHER:
            return "Other";

        case proto::DmiChassis::TYPE_DESKTOP:
            return "Desktop";

        case proto::DmiChassis::TYPE_LOW_PROFILE_DESKTOP:
            return "Low Profile Desktop";

        case proto::DmiChassis::TYPE_PIZZA_BOX:
            return "Pizza Box";

        case proto::DmiChassis::TYPE_MINI_TOWER:
            return "Mini Tower";

        case proto::DmiChassis::TYPE_TOWER:
            return "Tower";

        case proto::DmiChassis::TYPE_PORTABLE:
            return "Portable";

        case proto::DmiChassis::TYPE_LAPTOP:
            return "Laptop";

        case proto::DmiChassis::TYPE_NOTEBOOK:
            return "Notebook";

        case proto::DmiChassis::TYPE_HAND_HELD:
            return "Hand Held";

        case proto::DmiChassis::TYPE_DOCKING_STATION:
            return "Docking Station";

        case proto::DmiChassis::TYPE_ALL_IN_ONE:
            return "All In One";

        case proto::DmiChassis::TYPE_SUB_NOTEBOOK:
            return "Sub Notebook";

        case proto::DmiChassis::TYPE_SPACE_SAVING:
            return "Space Saving";

        case proto::DmiChassis::TYPE_LUNCH_BOX:
            return "Lunch Box";

        case proto::DmiChassis::TYPE_MAIN_SERVER_CHASSIS:
            return "Main Server Chassis";

        case proto::DmiChassis::TYPE_EXPANSION_CHASSIS:
            return "Expansion Chassis";

        case proto::DmiChassis::TYPE_SUB_CHASSIS:
            return "Sub Chassis";

        case proto::DmiChassis::TYPE_BUS_EXPANSION_CHASSIS:
            return "Bus Expansion Chassis";

        case proto::DmiChassis::TYPE_PERIPHERIAL_CHASSIS:
            return "Peripherial Chassis";

        case proto::DmiChassis::TYPE_RAID_CHASSIS:
            return "RAID Chassis";

        case proto::DmiChassis::TYPE_RACK_MOUNT_CHASSIS:
            return "Rack Mount Chassis";

        case proto::DmiChassis::TYPE_SEALED_CASE_PC:
            return "Sealed Case PC";

        case proto::DmiChassis::TYPE_MULTI_SYSTEM_CHASSIS:
            return "Multi System Chassis";

        case proto::DmiChassis::TYPE_COMPACT_PCI:
            return "Compact PCI";

        case proto::DmiChassis::TYPE_ADVANCED_TCA:
            return "Advanced TCA";

        case proto::DmiChassis::TYPE_BLADE:
            return "Blade";

        case proto::DmiChassis::TYPE_BLADE_ENCLOSURE:
            return "Blade Enclosure";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiChassis::StatusToString(proto::DmiChassis::Status status)
{
    switch (status)
    {
        case proto::DmiChassis::STATUS_OTHER:
            return "Other";

        case proto::DmiChassis::STATUS_SAFE:
            return "Safe";

        case proto::DmiChassis::STATUS_WARNING:
            return "Warning";

        case proto::DmiChassis::STATUS_CRITICAL:
            return "Critical";

        case proto::DmiChassis::STATUS_NON_RECOVERABLE:
            return "Non Recoverable";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiChassis::SecurityStatusToString(proto::DmiChassis::SecurityStatus status)
{
    switch (status)
    {
        case proto::DmiChassis::SECURITY_STATUS_OTHER:
            return "Other";

        case proto::DmiChassis::SECURITY_STATUS_NONE:
            return "None";

        case proto::DmiChassis::SECURITY_STATUS_EXTERNAL_INTERFACE_LOCKED_OUT:
            return "External Interface Locked Out";

        case proto::DmiChassis::SECURITY_STATUS_EXTERNAL_INTERFACE_ENABLED:
            return "External Interface Enabled";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiCaches
//

const char* CategoryDmiCaches::Name() const
{
    return "Caches";
}

Category::IconId CategoryDmiCaches::Icon() const
{
    return IDI_CHIP;
}

const char* CategoryDmiCaches::Guid() const
{
    return "{BA9258E7-0046-4A77-A97B-0407453706A3}";
}

void CategoryDmiCaches::Parse(Table& table, const std::string& data)
{
    proto::DmiCaches message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiCaches::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Cache #%d", index + 1));

        if (!item.name().empty())
            group.AddParam("Name", Value::String(item.name()));

        group.AddParam("Location", Value::String(LocationToString(item.location())));
        group.AddParam("Status", Value::String(StatusToString(item.status())));
        group.AddParam("Mode", Value::String(ModeToString(item.mode())));

        if (item.level() != 0)
            group.AddParam("Level", Value::FormattedString("L%d", item.level()));

        if (item.maximum_size() != 0)
            group.AddParam("Maximum Size", Value::Number(item.maximum_size(), "kB"));

        if (item.current_size() != 0)
            group.AddParam("Current Size", Value::Number(item.current_size(), "kB"));

        if (item.supported_sram_types())
        {
            Group types_group = group.AddGroup("Supported SRAM Types");

            types_group.AddParam("Non-burst",
                                 Value::Bool(item.supported_sram_types() &
                                                 proto::DmiCaches::SRAM_TYPE_NON_BURST));

            types_group.AddParam("Burst",
                                 Value::Bool(item.supported_sram_types() &
                                             proto::DmiCaches::SRAM_TYPE_BURST));

            types_group.AddParam("Pipeline Burst",
                                 Value::Bool(item.supported_sram_types() &
                                             proto::DmiCaches::SRAM_TYPE_PIPELINE_BURST));

            types_group.AddParam("Synchronous",
                                 Value::Bool(item.supported_sram_types() &
                                             proto::DmiCaches::SRAM_TYPE_SYNCHRONOUS));

            types_group.AddParam("Asynchronous",
                                 Value::Bool(item.supported_sram_types() &
                                             proto::DmiCaches::SRAM_TYPE_ASYNCHRONOUS));
        }

        group.AddParam("Current SRAM Type", Value::String(SRAMTypeToString(item.current_sram_type())));

        if (item.speed() != 0)
            group.AddParam("Speed", Value::Number(item.speed(), "ns"));

        group.AddParam("Error Correction Type", Value::String(ErrorCorrectionTypeToString(item.error_correction_type())));
        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("Associativity", Value::String(AssociativityToString(item.associativity())));
    }
}

std::string CategoryDmiCaches::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiCaches message;

    for (SMBios::TableEnumerator<SMBios::CacheTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::CacheTable table = table_enumerator.GetTable();
        proto::DmiCaches::Item* item = message.add_item();

        item->set_name(table.GetName());
        item->set_location(table.GetLocation());
        item->set_status(table.GetStatus());
        item->set_mode(table.GetMode());
        item->set_level(table.GetLevel());
        item->set_maximum_size(table.GetMaximumSize());
        item->set_current_size(table.GetCurrentSize());
        item->set_supported_sram_types(table.GetSupportedSRAMTypes());
        item->set_current_sram_type(table.GetCurrentSRAMType());
        item->set_speed(table.GetSpeed());
        item->set_error_correction_type(table.GetErrorCorrectionType());
        item->set_type(table.GetType());
        item->set_associativity(table.GetAssociativity());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiCaches::LocationToString(proto::DmiCaches::Location value)
{
    switch (value)
    {
        case proto::DmiCaches::LOCATION_INTERNAL:
            return "Internal";

        case proto::DmiCaches::LOCATION_EXTERNAL:
            return "External";

        case proto::DmiCaches::LOCATION_RESERVED:
            return "Reserved";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiCaches::StatusToString(proto::DmiCaches::Status value)
{
    switch (value)
    {
        case proto::DmiCaches::STATUS_ENABLED:
            return "Enabled";

        case proto::DmiCaches::STATUS_DISABLED:
            return "Disabled";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiCaches::ModeToString(proto::DmiCaches::Mode value)
{
    switch (value)
    {
        case proto::DmiCaches::MODE_WRITE_THRU:
            return "Write Thru";

        case proto::DmiCaches::MODE_WRITE_BACK:
            return "Write Back";

        case proto::DmiCaches::MODE_WRITE_WITH_MEMORY_ADDRESS:
            return "Write with memory address";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiCaches::SRAMTypeToString(proto::DmiCaches::SRAMType value)
{
    switch (value)
    {
        case proto::DmiCaches::SRAM_TYPE_OTHER:
            return "Other";

        case proto::DmiCaches::SRAM_TYPE_UNKNOWN:
            return "Unknown";

        case proto::DmiCaches::SRAM_TYPE_NON_BURST:
            return "Non-burst";

        case proto::DmiCaches::SRAM_TYPE_BURST:
            return "Burst";

        case proto::DmiCaches::SRAM_TYPE_PIPELINE_BURST:
            return "Pipeline Burst";

        case proto::DmiCaches::SRAM_TYPE_SYNCHRONOUS:
            return "Synchronous";

        case proto::DmiCaches::SRAM_TYPE_ASYNCHRONOUS:
            return "Asynchronous";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiCaches::ErrorCorrectionTypeToString(
    proto::DmiCaches::ErrorCorrectionType value)
{
    switch (value)
    {
        case proto::DmiCaches::ERROR_CORRECTION_TYPE_OTHER:
            return "Other";

        case proto::DmiCaches::ERROR_CORRECTION_TYPE_NONE:
            return "None";

        case proto::DmiCaches::ERROR_CORRECTION_TYPE_PARITY:
            return "Parity";

        case proto::DmiCaches::ERROR_CORRECTION_TYPE_SINGLE_BIT_ECC:
            return "Single bit ECC";

        case proto::DmiCaches::ERROR_CORRECTION_TYPE_MULTI_BIT_ECC:
            return "Multi bit ECC";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiCaches::TypeToString(proto::DmiCaches::Type value)
{
    switch (value)
    {
        case proto::DmiCaches::TYPE_OTHER:
            return "Other";

        case proto::DmiCaches::TYPE_INSTRUCTION:
            return "Instruction";

        case proto::DmiCaches::TYPE_DATA:
            return "Data";

        case proto::DmiCaches::TYPE_UNIFIED:
            return "Unified";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiCaches::AssociativityToString(proto::DmiCaches::Associativity value)
{
    switch (value)
    {
        case proto::DmiCaches::ASSOCIATIVITY_OTHER:
            return "Other";

        case proto::DmiCaches::ASSOCIATIVITY_DIRECT_MAPPED:
            return "Direct Mapped";

        case proto::DmiCaches::ASSOCIATIVITY_2_WAY:
            return "2-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_4_WAY:
            return "4-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_FULLY:
            return "Fully Associative";

        case proto::DmiCaches::ASSOCIATIVITY_8_WAY:
            return "8-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_16_WAY:
            return "16-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_12_WAY:
            return "12-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_24_WAY:
            return "24-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_32_WAY:
            return "32-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_48_WAY:
            return "48-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_64_WAY:
            return "64-way Set-Associative";

        case proto::DmiCaches::ASSOCIATIVITY_20_WAY:
            return "20-way Set-Associative";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiProcessors
//

const char* CategoryDmiProcessors::Name() const
{
    return "Processors";
}

Category::IconId CategoryDmiProcessors::Icon() const
{
    return IDI_PROCESSOR;
}

const char* CategoryDmiProcessors::Guid() const
{
    return "{84D8B0C3-37A4-4825-A523-40B62E0CADC3}";
}

void CategoryDmiProcessors::Parse(Table& table, const std::string& data)
{
    proto::DmiProcessors message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiProcessors::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Processor #%d", index + 1));

        if (!item.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufacturer()));

        if (!item.version().empty())
            group.AddParam("Version", Value::String(item.version()));

        group.AddParam("Family", Value::String(FamilyToString(item.family())));
        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("Status", Value::String(StatusToString(item.status())));

        if (!item.socket().empty())
            group.AddParam("Socket", Value::String(item.socket()));

        group.AddParam("Upgrade", Value::String(UpgradeToString(item.upgrade())));

        if (item.external_clock() != 0)
            group.AddParam("External Clock", Value::Number(item.external_clock(), "MHz"));

        if (item.current_speed() != 0)
            group.AddParam("Current Speed", Value::Number(item.current_speed(), "MHz"));

        if (item.maximum_speed() != 0)
            group.AddParam("Maximum Speed", Value::Number(item.maximum_speed(), "MHz"));

        if (item.voltage() != 0.0)
            group.AddParam("Voltage", Value::Number(item.voltage(), "V"));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.asset_tag().empty())
            group.AddParam("Asset Tag", Value::String(item.asset_tag()));

        if (!item.part_number().empty())
            group.AddParam("Part Number", Value::String(item.part_number()));

        if (item.core_count() != 0)
            group.AddParam("Core Count", Value::Number(item.core_count()));

        if (item.core_enabled() != 0)
            group.AddParam("Core Enabled", Value::Number(item.core_enabled()));

        if (item.thread_count() != 0)
            group.AddParam("Thread Count", Value::Number(item.thread_count()));

        if (item.characteristics())
        {
            Group features_group = group.AddGroup("Features");

            auto add_feature = [&](const char* name, uint32_t flag)
            {
                features_group.AddParam(name, Value::Bool(item.characteristics() & flag));
            };

            add_feature("64-bit Capable", proto::DmiProcessors::CHARACTERISTIC_64BIT_CAPABLE);
            add_feature("Multi-Core", proto::DmiProcessors::CHARACTERISTIC_MULTI_CORE);
            add_feature("Hardware Thread", proto::DmiProcessors::CHARACTERISTIC_HARDWARE_THREAD);
            add_feature("Execute Protection",proto::DmiProcessors::CHARACTERISTIC_EXECUTE_PROTECTION);
            add_feature("Enhanced Virtualization", proto::DmiProcessors::CHARACTERISTIC_ENHANCED_VIRTUALIZATION);
            add_feature("Power/Perfomance Control", proto::DmiProcessors::CHARACTERISTIC_POWER_CONTROL);
        }
    }
}

std::string CategoryDmiProcessors::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiProcessors message;

    for (SMBios::TableEnumerator<SMBios::ProcessorTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::ProcessorTable table = table_enumerator.GetTable();
        proto::DmiProcessors::Item* item = message.add_item();

        item->set_manufacturer(table.GetManufacturer());
        item->set_version(table.GetVersion());
        item->set_family(table.GetFamily());
        item->set_type(table.GetType());
        item->set_status(table.GetStatus());
        item->set_socket(table.GetSocket());
        item->set_upgrade(table.GetUpgrade());
        item->set_external_clock(table.GetExternalClock());
        item->set_current_speed(table.GetCurrentSpeed());
        item->set_maximum_speed(table.GetMaximumSpeed());
        item->set_voltage(table.GetVoltage());
        item->set_serial_number(table.GetSerialNumber());
        item->set_asset_tag(table.GetAssetTag());
        item->set_part_number(table.GetPartNumber());
        item->set_core_count(table.GetCoreCount());
        item->set_core_enabled(table.GetCoreEnabled());
        item->set_thread_count(table.GetThreadCount());
        item->set_characteristics(table.GetCharacteristics());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiProcessors::FamilyToString(proto::DmiProcessors::Family value)
{
    switch (value)
    {
        case proto::DmiProcessors::FAMILY_OTHER:
            return "Other";

        case proto::DmiProcessors::FAMILY_8086:
            return "8086";

        case proto::DmiProcessors::FAMILY_80286:
            return "80286";

        case proto::DmiProcessors::FAMILY_INTEL_386_PROCESSOR:
            return "Intel386";

        case proto::DmiProcessors::FAMILY_INTEL_486_PROCESSOR:
            return "Intel486";

        case proto::DmiProcessors::FAMILY_8087:
            return "8087";

        case proto::DmiProcessors::FAMILY_80287:
            return "80287";

        case proto::DmiProcessors::FAMILY_80387:
            return "80387";

        case proto::DmiProcessors::FAMILY_80487:
            return "80487";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_PROCESSOR:
            return "Intel Pentium";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_PRO_PROCESSOR:
            return "Intel Pentium Pro";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_2_PROCESSOR:
            return "Intel Pentium II";

        case proto::DmiProcessors::FAMILY_PENTIUM_PROCESSOR_WITH_MMX:
            return "Intel Pentium with MMX Technology";

        case proto::DmiProcessors::FAMILY_INTEL_CELERON_PROCESSOR:
            return "Intel Celeron";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_2_XEON_PROCESSOR:
            return "Intel Pentium II Xeon";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_3_PROCESSOR:
            return "Intel Pentium III";

        case proto::DmiProcessors::FAMILY_M1_FAMILY:
            return "M1";

        case proto::DmiProcessors::FAMILY_M2_FAMILY:
            return "M2";

        case proto::DmiProcessors::FAMILY_INTEL_CELEROM_M_PROCESSOR:
            return "Intel Celeron M";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_4_HT_PROCESSOR:
            return "Intel Pentium 4 HT";

        case proto::DmiProcessors::FAMILY_AMD_DURON_PROCESSOR_FAMILY:
            return "AMD Duron";

        case proto::DmiProcessors::FAMILY_AMD_K5_FAMILY:
            return "AMD K5";

        case proto::DmiProcessors::FAMILY_AMD_K6_FAMILY:
            return "AMD K6";

        case proto::DmiProcessors::FAMILY_AMD_K6_2:
            return "AMD K6-2";

        case proto::DmiProcessors::FAMILY_AMD_K6_3:
            return "AMD K6-3";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_PROCESSOR_FAMILY:
            return "AMD Athlon";

        case proto::DmiProcessors::FAMILY_AMD_29000_FAMILY:
            return "AMD 29000";

        case proto::DmiProcessors::FAMILY_AMD_K6_2_PLUS:
            return "AMD K6-2+";

        case proto::DmiProcessors::FAMILY_POWER_PC_FAMILY:
            return "Power PC";

        case proto::DmiProcessors::FAMILY_POWER_PC_601:
            return "Power PC 601";

        case proto::DmiProcessors::FAMILY_POWER_PC_603:
            return "Power PC 603";

        case proto::DmiProcessors::FAMILY_POWER_PC_603_PLUS:
            return "Power PC 603+";

        case proto::DmiProcessors::FAMILY_POWER_PC_604:
            return "Power PC 604";

        case proto::DmiProcessors::FAMILY_POWER_PC_620:
            return "Power PC 620";

        case proto::DmiProcessors::FAMILY_POWER_PC_X704:
            return "Power PC x704";

        case proto::DmiProcessors::FAMILY_POWER_PC_750:
            return "Power PC 750";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_DUO_PROCESSOR:
            return "Intel Core Duo";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_DUO_MOBILE_PROCESSOR:
            return "Intel Core Duo Mobile";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_SOLO_MOBILE_PROCESSOR:
            return "Intel Core Solo Mobile";

        case proto::DmiProcessors::FAMILY_INTEL_ATOM_PROCESSOR:
            return "Intel Atom";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_M_PROCESSOR:
            return "Intel Core M";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_M3_PROCESSOR:
            return "Intel Core m3";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_M5_PROCESSOR:
            return "Intel Core m5";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_M7_PROCESSOR:
            return "Intel Core m7";

        case proto::DmiProcessors::FAMILY_ALPHA_FAMILY:
            return "Alpha";

        case proto::DmiProcessors::FAMILY_ALPHA_21064:
            return "Alpha 21064";

        case proto::DmiProcessors::FAMILY_ALPHA_21066:
            return "Alpha 21066";

        case proto::DmiProcessors::FAMILY_ALPHA_21164:
            return "Alpha 21164";

        case proto::DmiProcessors::FAMILY_ALPHA_21164PC:
            return "Alpha 21164PC";

        case proto::DmiProcessors::FAMILY_ALPHA_21164A:
            return "Alpha 21164a";

        case proto::DmiProcessors::FAMILY_ALPHA_21264:
            return "Alpha 21264";

        case proto::DmiProcessors::FAMILY_ALPHA_21364:
            return "Alpha 21364";

        case proto::DmiProcessors::FAMILY_AMD_TURION_2_ULTRA_DUAL_CORE_MOBILE_M_FAMILY:
            return "AMD Turion II Ultra Dual-Core Mobile M";

        case proto::DmiProcessors::FAMILY_AMD_TURION_2_DUAL_CORE_MOBILE_M_FAMILY:
            return "AMD Turion II Dual-Core Mobile M";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_2_DUAL_CORE_M_FAMILY:
            return "AMD Athlon II Dual-Core M";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_6100_SERIES_PROCESSOR:
            return "AMD Opteron 6100";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_4100_SERIES_PROCESSOR:
            return "AMD Opteron 4100";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_6200_SERIES_PROCESSOR:
            return "AMD Opteron 6200";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_4200_SERIES_PROCESSOR:
            return "AMD Opteron 4200";

        case proto::DmiProcessors::FAMILY_AMD_FX_SERIES_PROCESSOR:
            return "AMD FX Series";

        case proto::DmiProcessors::FAMILY_MIPS_FAMILY:
            return "MIPS";

        case proto::DmiProcessors::FAMILY_MIPS_R4000:
            return "MIPS R4000";

        case proto::DmiProcessors::FAMILY_MIPS_R4200:
            return "MIPS R4200";

        case proto::DmiProcessors::FAMILY_MIPS_R4400:
            return "MIPS R4400";

        case proto::DmiProcessors::FAMILY_MIPS_R4600:
            return "MIPS R4600";

        case proto::DmiProcessors::FAMILY_MIPS_R10000:
            return "MIPS R10000";

        case proto::DmiProcessors::FAMILY_AMD_C_SERIES_PROCESSOR:
            return "AMD C-Series";

        case proto::DmiProcessors::FAMILY_AMD_E_SERIES_PROCESSOR:
            return "AMD E-Series";

        case proto::DmiProcessors::FAMILY_AMD_A_SERIES_PROCESSOR:
            return "AMD A-Series";

        case proto::DmiProcessors::FAMILY_AMD_G_SERIES_PROCESSOR:
            return "AMD G-Series";

        case proto::DmiProcessors::FAMILY_AMD_Z_SERIES_PROCESSOR:
            return "AMD Z-Series";

        case proto::DmiProcessors::FAMILY_AMD_R_SERIES_PROCESSOR:
            return "AMD R-Series";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_4300_SERIES_PROCESSOR:
            return "AMD Opteron 4300";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_6300_SERIES_PROCESSOR:
            return "AMD Opteron 6300";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_3300_SERIES_PROCESSOR:
            return "AMD Opteron 3300";

        case proto::DmiProcessors::FAMILY_AMD_FIREPRO_SERIES_PROCESSOR:
            return "AMD FirePro";

        case proto::DmiProcessors::FAMILY_SPARC_FAMILY:
            return "SPARC";

        case proto::DmiProcessors::FAMILY_SUPER_SPARC:
            return "SuperSPARC";

        case proto::DmiProcessors::FAMILY_MICRO_SPARC_2:
            return "microSPARC II";

        case proto::DmiProcessors::FAMILY_MICRO_SPARC_2EP:
            return "microSPARC IIep";

        case proto::DmiProcessors::FAMILY_ULTRA_SPARC:
            return "UltraSPARC";

        case proto::DmiProcessors::FAMILY_ULTRA_SPARC_2:
            return "UltraSPARC II";

        case proto::DmiProcessors::FAMILY_ULTRA_SPARC_2I:
            return "UltraSPARC IIi";

        case proto::DmiProcessors::FAMILY_ULTRA_SPARC_3:
            return "UltraSPARC III";

        case proto::DmiProcessors::FAMILY_ULTRA_SPARC_3I:
            return "UltraSPARC IIIi";

        case proto::DmiProcessors::FAMILY_68040_FAMILY:
            return "68040";

        case proto::DmiProcessors::FAMILY_68XXX:
            return "68xxx";

        case proto::DmiProcessors::FAMILY_68000:
            return "68000";

        case proto::DmiProcessors::FAMILY_68010:
            return "68010";

        case proto::DmiProcessors::FAMILY_68020:
            return "68020";

        case proto::DmiProcessors::FAMILY_68030:
            return "68030";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_X4_QUAD_CORE_PROCESSOR_FAMILY:
            return "AMD Athlon X4 Quad-Core";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_X1000_SERIES_PROCESSOR:
            return "AMD Opteron X1000 Series Processor";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_X2000_SERIES_APU:
            return "AMD Opteron X2000 Series APU";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_A_SERIES_PROCESSOR:
            return "AMD Opteron A-Series Processor";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_X3000_SERIES_APU:
            return "AMD Opteron X3000 Series APU";

        case proto::DmiProcessors::FAMILY_AMD_ZEN_PROCESSOR_FAMILY:
            return "AMD Zen";

        case proto::DmiProcessors::FAMILY_HOBBIT_FAMILY:
            return "Hobbit";

        case proto::DmiProcessors::FAMILY_CRUSOE_TM5000_FAMILY:
            return "Crusoe TM5000";

        case proto::DmiProcessors::FAMILY_CRUSOE_TM3000_FAMILY:
            return "Crusoe TM3000";

        case proto::DmiProcessors::FAMILY_EFFICEON_TM8000_FAMILY:
            return "Efficeon TM8000";

        case proto::DmiProcessors::FAMILY_WEITEK:
            return "Weitek";

        case proto::DmiProcessors::FAMILY_INTEL_ITANIUM_PROCESSOR:
            return "Intel Itanium";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_64_PROCESSOR_FAMILY:
            return "AMD Athlon 64";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_PROCESSOR_FAMILY:
            return "AMD Opteron";

        case proto::DmiProcessors::FAMILY_AMD_SEMPRON_PROCESSOR_FAMILY:
            return "AMD Sempron";

        case proto::DmiProcessors::FAMILY_AMD_TURION_64_MOBILE_TECHNOLOGY:
            return "AMD Turion 64 Mobile Technology";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Opteron Dual-Core";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_64_X2_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Athlon 64 X2 Dual-Core";

        case proto::DmiProcessors::FAMILY_AMD_TURION_64_X2_MOBILE_TECHNOLOGY:
            return "AMD Turion 64 X2 Mobile Technology";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_QUAD_CORE_PROCESSOR_FAMILY:
            return "AMD Opteron Quad-Core";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_THIRD_GEN_PROCESSOR_FAMILY:
            return "AMD Opteron Third-Generation";

        case proto::DmiProcessors::FAMILY_AMD_PHENOM_FX_QUAD_CORE_PROCESSOR_FAMILY:
            return "AMD Phenom FX Quad-Core";

        case proto::DmiProcessors::FAMILY_AMD_PHENOM_X4_QUAD_CORE_PROCESSOR_FAMILY:
            return "AMD Phenom X4 Quad-Core";

        case proto::DmiProcessors::FAMILY_AMD_PHENOM_X2_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Phenom X2 Dual-Core";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_X2_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Athlon X2 Dual-Core";

        case proto::DmiProcessors::FAMILY_PA_RISC_FAMILY:
            return "PA-RISC";

        case proto::DmiProcessors::FAMILY_PA_RISC_8500:
            return "PA-RISC 8500";

        case proto::DmiProcessors::FAMILY_PA_RISC_8000:
            return "PA-RISC 8000";

        case proto::DmiProcessors::FAMILY_PA_RISC_7300LC:
            return "PA-RISC 7300LC";

        case proto::DmiProcessors::FAMILY_PA_RISC_7200:
            return "PA-RISC 7200";

        case proto::DmiProcessors::FAMILY_PA_RISC_7100LC:
            return "PA-RISC 7100LC";

        case proto::DmiProcessors::FAMILY_PA_RISC_7100:
            return "PA-RISC 7100";

        case proto::DmiProcessors::FAMILY_V30_FAMILY:
            return "V30";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_3200_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 3200";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_3000_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 3000";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_5300_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 5300";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_5100_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 5100";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_5000_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 5000";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_LV_PROCESSOR:
            return "Intel Xeon Dual-Core LV";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_ULV_PROCESSOR:
            return "Intel Xeon Dual-Core ULV";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_7100_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 7100";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_5400_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 5400";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_PROCESSOR:
            return "Intel Xeon Quad-Core";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_5200_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 5200";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_7200_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 7200";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_7300_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 7300";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_7400_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 7400";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_MULTI_CORE_7400_PROCESSOR_SERIES:
            return "Intel Xeon Multi-Core 7400";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_3_XEON_PROCESSOR:
            return "Intel Pentium III Xeon";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_3_PROCESSOR_WITH_SPEED_STEP:
            return "Intel Pentium III with SpeedStep Technology";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_4_PROCESSOR:
            return "Intel Pentium 4";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_PROCESSOR:
            return "Intel Xeon";

        case proto::DmiProcessors::FAMILY_AS400_FAMILY:
            return "AS400";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_MP_PROCESSOR:
            return "Intel Xeon MP";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_XP_PROCESSOR_FAMILY:
            return "AMD Athlon XP";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_MP_PROCESSOR_FAMILY:
            return "AMD Athlon MP";

        case proto::DmiProcessors::FAMILY_INTEL_ITANIUM_2_PROCESSOR:
            return "Intel Itanium 2";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_M_PROCESSOR:
            return "Intel Pentium M";

        case proto::DmiProcessors::FAMILY_INTEL_CELERON_D_PROCESSOR:
            return "Intel Celeron D";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_D_PROCESSOR:
            return "Intel Pentium D";

        case proto::DmiProcessors::FAMILY_INTEL_PENTIUM_PROCESSOR_EXTREME_EDITION:
            return "Intel Pentium Extreme Edition";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_SOLO_PROCESSOR:
            return "Intel Core Solo";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_2_DUO_PROCESSOR:
            return "Intel Core 2 Duo";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_2_SOLO_PROCESSOR:
            return "Intel Core 2 Solo";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_2_EXTREME_PROCESSOR:
            return "Intel Core 2 Extreme";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_2_QUAD_PROCESSOR:
            return "Intel Core 2 Quad";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_2_EXTREME_MOBILE_PROCESSOR:
            return "Intel Core 2 Extreme Mobile";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_2_DUO_MOBILE_PROCESSOR:
            return "Intel Core 2 Duo Mobile";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_2_SOLO_MOBILE_PROCESSOR:
            return "Intel Core 2 Solo Mobile";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_I7_PROCESSOR:
            return "Intel Core i7";

        case proto::DmiProcessors::FAMILY_INTEL_CELERON_DUAL_CORE_PROCESSOR:
            return "Intel Celeron Dual-Core";

        case proto::DmiProcessors::FAMILY_IBM390_FAMILY:
            return "IBM390";

        case proto::DmiProcessors::FAMILY_G4:
            return "G4";

        case proto::DmiProcessors::FAMILY_G5:
            return "G5";

        case proto::DmiProcessors::FAMILY_ESA_390_G6:
            return "ESA/390 G6";

        case proto::DmiProcessors::FAMILY_Z_ARCHITECTURE_BASE:
            return "z/Architecture Base";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_I5_PROCESSOR:
            return "Intel Core i5";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_I3_PROCESSOR:
            return "Intel Core i3";

        case proto::DmiProcessors::FAMILY_VIA_C7_M_PROCESSOR_FAMILY:
            return "VIA C7-M";

        case proto::DmiProcessors::FAMILY_VIA_C7_D_PROCESSOR_FAMILY:
            return "VIA C7-D";

        case proto::DmiProcessors::FAMILY_VIA_C7_PROCESSOR_FAMILY:
            return "VIA C7";

        case proto::DmiProcessors::FAMILY_VIA_EDEN_PROCESSOR_FAMILY:
            return "VIA Eden";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_MULTI_CORE_PROCESSOR:
            return "Intel Xeon Multi-Core";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_3XXX_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 3xxx";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_3XXX_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 3xxx";

        case proto::DmiProcessors::FAMILY_VIA_NANO_PROCESSOR_FAMILY:
            return "VIA Nano";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_5XXX_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 5xxx";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_5XXX_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 5xxx";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_DUAL_CORE_7XXX_PROCESSOR_SERIES:
            return "Intel Xeon Dual-Core 7xxx";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_QUAD_CORE_7XXX_PROCESSOR_SERIES:
            return "Intel Xeon Quad-Core 7xxx";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_MULTI_CORE_7XXX_PROCESSOR_SERIES:
            return "Intel Xeon Multi-Core 7xxx";

        case proto::DmiProcessors::FAMILY_INTEL_XEON_MULTI_CORE_3400_PROCESSOR_SERIES:
            return "Intel Xeon Multi-Core 3400";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_3000_PROCESSOR_SERIES:
            return "AMD Opteron 3000 Series";

        case proto::DmiProcessors::FAMILY_AMD_SEMPRON_II_PROCESSOR:
            return "AMD Sempron II";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_QUAD_CORE_EMBEDDED_PROCESSOR_FAMILY:
            return "AMD Opteron Quad-Core Embedded";

        case proto::DmiProcessors::FAMILY_AMD_PHENOM_TRIPLE_CORE_PROCESSOR_FAMILY:
            return "AMD Phenom Triple-Core";

        case proto::DmiProcessors::FAMILY_AMD_TURION_ULTRA_DUAL_CORE_MOBILE_PROCESSOR_FAMILY:
            return "AMD Turion Ultra Dual-Core Mobile";

        case proto::DmiProcessors::FAMILY_AMD_TURION_DUAL_CORE_MOBILE_PROCESSOR_FAMILY:
            return "AMD Turion Dual-Core Mobile";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_DUAL_CORE_PROCESSOR_FAMILY:
            return "AMD Athlon Dual-Core";

        case proto::DmiProcessors::FAMILY_AMD_SEMPRON_SI_PROCESSOR_FAMILY:
            return "AMD Sempron SI";

        case proto::DmiProcessors::FAMILY_AMD_PHENOM_2_PROCESSOR_FAMILY:
            return "AMD Phenom II";

        case proto::DmiProcessors::FAMILY_AMD_ATHLON_2_PROCESSOR_FAMILY:
            return "AMD Athlon II";

        case proto::DmiProcessors::FAMILY_AMD_OPTERON_SIX_CORE_PROCESSOR_FAMILY:
            return "AMD Opteron Six-Core";

        case proto::DmiProcessors::FAMILY_AMD_SEMPRON_M_PROCESSOR_FAMILY:
            return "AMD Sempron M";

        case proto::DmiProcessors::FAMILY_I860:
            return "i860";

        case proto::DmiProcessors::FAMILY_I960:
            return "i960";

        case proto::DmiProcessors::FAMILY_ARM_V7:
            return "ARMv7";

        case proto::DmiProcessors::FAMILY_ARM_V8:
            return "ARMv8";

        case proto::DmiProcessors::FAMILY_SH_3:
            return "SH-3";

        case proto::DmiProcessors::FAMILY_SH_4:
            return "SH-4";

        case proto::DmiProcessors::FAMILY_ARM:
            return "ARM";

        case proto::DmiProcessors::FAMILY_STRONG_ARM:
            return "StrongARM";

        case proto::DmiProcessors::FAMILY_6X86:
            return "6x86";

        case proto::DmiProcessors::FAMILY_MEDIA_GX:
            return "MediaGX";

        case proto::DmiProcessors::FAMILY_MII:
            return "MII";

        case proto::DmiProcessors::FAMILY_WIN_CHIP:
            return "WinChip";

        case proto::DmiProcessors::FAMILY_DSP:
            return "DSP";

        case proto::DmiProcessors::FAMILY_VIDEO_PROCESSOR:
            return "Video Processor";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_2_FAMILY:
            return "Intel Core 2";

        case proto::DmiProcessors::FAMILY_AMD_K7_FAMILY:
            return "AMD K7";

        case proto::DmiProcessors::FAMILY_INTEL_CORE_2_OR_AMD_K7_FAMILY:
            return "Intel Core 2 or AMD K7";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiProcessors::TypeToString(proto::DmiProcessors::Type value)
{
    switch (value)
    {
        case proto::DmiProcessors::TYPE_CENTRAL_PROCESSOR:
            return "Central Processor";

        case proto::DmiProcessors::TYPE_MATH_PROCESSOR:
            return "Math Processor";

        case proto::DmiProcessors::TYPE_DSP_PROCESSOR:
            return "DSP Processor";

        case proto::DmiProcessors::TYPE_VIDEO_PROCESSOR:
            return "Video Processor";

        case proto::DmiProcessors::TYPE_OTHER:
            return "Other Processor";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiProcessors::StatusToString(proto::DmiProcessors::Status value)
{
    switch (value)
    {
        case proto::DmiProcessors::STATUS_ENABLED:
            return "Enabled";

        case proto::DmiProcessors::STATUS_DISABLED_BY_USER:
            return "Disabled by User";

        case proto::DmiProcessors::STATUS_DISABLED_BY_BIOS:
            return "Disabled by BIOS";

        case proto::DmiProcessors::STATUS_IDLE:
            return "Idle";

        case proto::DmiProcessors::STATUS_OTHER:
            return "Other";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiProcessors::UpgradeToString(proto::DmiProcessors::Upgrade value)
{
    switch (value)
    {
        case proto::DmiProcessors::UPGRADE_OTHER:
            return "Other";

        case proto::DmiProcessors::UPGRADE_DAUGHTER_BOARD:
            return "Daughter Board";

        case proto::DmiProcessors::UPGRADE_ZIF_SOCKET:
            return "ZIF Socket";

        case proto::DmiProcessors::UPGRADE_REPLACEABLE_PIGGY_BACK:
            return "Replaceable Piggy Back";

        case proto::DmiProcessors::UPGRADE_NONE:
            return "None";

        case proto::DmiProcessors::UPGRADE_LIF_SOCKET:
            return "LIF Socket";

        case proto::DmiProcessors::UPGRADE_SLOT_1:
            return "Slot 1";

        case proto::DmiProcessors::UPGRADE_SLOT_2:
            return "Slot 2";

        case proto::DmiProcessors::UPGRADE_370_PIN_SOCKET:
            return "370-pin Socket";

        case proto::DmiProcessors::UPGRADE_SLOT_A:
            return "Slot A";

        case proto::DmiProcessors::UPGRADE_SLOT_M:
            return "Slot M";

        case proto::DmiProcessors::UPGRADE_SOCKET_423:
            return "Socket 423";

        case proto::DmiProcessors::UPGRADE_SOCKET_462:
            return "Socket A (Socket 462)";

        case proto::DmiProcessors::UPGRADE_SOCKET_478:
            return "Socket 478";

        case proto::DmiProcessors::UPGRADE_SOCKET_754:
            return "Socket 754";

        case proto::DmiProcessors::UPGRADE_SOCKET_940:
            return "Socket 940";

        case proto::DmiProcessors::UPGRADE_SOCKET_939:
            return "Socket 939";

        case proto::DmiProcessors::UPGRADE_SOCKET_MPGA604:
            return "Socket mPGA604";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA771:
            return "Socket LGA771";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA775:
            return "Socket LGA775";

        case proto::DmiProcessors::UPGRADE_SOCKET_S1:
            return "Socket S1";

        case proto::DmiProcessors::UPGRADE_SOCKET_AM2:
            return "Socket AM2";

        case proto::DmiProcessors::UPGRADE_SOCKET_F:
            return "Socket F (1207)";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA1366:
            return "Socket LGA1366";

        case proto::DmiProcessors::UPGRADE_SOCKET_G34:
            return "Socket G34";

        case proto::DmiProcessors::UPGRADE_SOCKET_AM3:
            return "Socket AM3";

        case proto::DmiProcessors::UPGRADE_SOCKET_C32:
            return "Socket C32";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA1156:
            return "Socket LGA1156";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA1567:
            return "Socket LGA1567";

        case proto::DmiProcessors::UPGRADE_SOCKET_PGA988A:
            return "Socket PGA988A";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1288:
            return "Socket BGA1288";

        case proto::DmiProcessors::UPGRADE_SOCKET_RPGA988B:
            return "Socket rPGA988B";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1023:
            return "Socket BGA1023";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1224:
            return "Socket BGA1224";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1155:
            return "Socket BGA1155";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA1356:
            return "Socket LGA1356";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA2011:
            return "Socket LGA2011";

        case proto::DmiProcessors::UPGRADE_SOCKET_FS1:
            return "Socket FS1";

        case proto::DmiProcessors::UPGRADE_SOCKET_FS2:
            return "Socket FS2";

        case proto::DmiProcessors::UPGRADE_SOCKET_FM1:
            return "Socket FM1";

        case proto::DmiProcessors::UPGRADE_SOCKET_FM2:
            return "Socket FM2";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA2011_3:
            return "Socket LGA2011-3";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA1356_3:
            return "Socket LGA1356-3";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA1150:
            return "Socket LGA1150";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1168:
            return "Socket BGA1168";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1234:
            return "Socket BGA1234";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1364:
            return "Socket BGA1364";

        case proto::DmiProcessors::UPGRADE_SOCKET_AM4:
            return "Socket AM4";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA1151:
            return "Socket LGA1151";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1356:
            return "Socket BGA1356";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1440:
            return "Socket BGA1440";

        case proto::DmiProcessors::UPGRADE_SOCKET_BGA1515:
            return "Socket BGA1515";

        case proto::DmiProcessors::UPGRADE_SOCKET_LGA3647_1:
            return "Socket LGA3647-1";

        case proto::DmiProcessors::UPGRADE_SOCKET_SP3:
            return "Socket SP3";

        case proto::DmiProcessors::UPGRADE_SOCKET_SP3_R2:
            return "Socket SP3r2";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiMemoryDevices
//

const char* CategoryDmiMemoryDevices::Name() const
{
    return "Memory Devices";
}

Category::IconId CategoryDmiMemoryDevices::Icon() const
{
    return IDI_MEMORY;
}

const char* CategoryDmiMemoryDevices::Guid() const
{
    return "{9C591459-A83F-4F48-883D-927765C072B0}";
}

void CategoryDmiMemoryDevices::Parse(Table& table, const std::string& data)
{
    proto::DmiMemoryDevices message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiMemoryDevices::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Memory Device #%d", index + 1));

        if (!item.device_locator().empty())
            group.AddParam("Device Locator", Value::String(item.device_locator()));

        if (item.size() != 0)
            group.AddParam("Size", Value::Number(item.size(), "MB"));

        group.AddParam("Type", Value::String(TypeToString(item.type())));

        if (item.speed() != 0)
            group.AddParam("Speed", Value::Number(item.speed(), "MHz"));

        group.AddParam("Form Factor", Value::String(FormFactorToString(item.form_factor())));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.part_number().empty())
            group.AddParam("Part Number", Value::String(item.part_number()));

        if (!item.manufactorer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufactorer()));

        if (!item.bank().empty())
            group.AddParam("Bank", Value::String(item.bank()));

        if (item.total_width() != 0)
            group.AddParam("Total Width", Value::Number(item.total_width(), "Bit"));

        if (item.data_width() != 0)
            group.AddParam("Data Width", Value::Number(item.data_width(), "Bit"));
    }
}

std::string CategoryDmiMemoryDevices::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiMemoryDevices message;

    for (SMBios::TableEnumerator<SMBios::MemoryDeviceTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::MemoryDeviceTable table = table_enumerator.GetTable();
        proto::DmiMemoryDevices::Item* item = message.add_item();

        item->set_device_locator(table.GetDeviceLocator());
        item->set_size(table.GetSize());
        item->set_type(table.GetType());
        item->set_speed(table.GetSpeed());
        item->set_form_factor(table.GetFormFactor());
        item->set_serial_number(table.GetSerialNumber());
        item->set_part_number(table.GetPartNumber());
        item->set_manufactorer(table.GetManufacturer());
        item->set_bank(table.GetBank());
        item->set_total_width(table.GetTotalWidth());
        item->set_data_width(table.GetDataWidth());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiMemoryDevices::TypeToString(proto::DmiMemoryDevices::Type value)
{
    switch (value)
    {
        case proto::DmiMemoryDevices::TYPE_OTHER:
            return "Other";

        case proto::DmiMemoryDevices::TYPE_DRAM:
            return "DRAM";

        case proto::DmiMemoryDevices::TYPE_EDRAM:
            return "EDRAM";

        case proto::DmiMemoryDevices::TYPE_VRAM:
            return "VRAM";

        case proto::DmiMemoryDevices::TYPE_SRAM:
            return "SRAM";

        case proto::DmiMemoryDevices::TYPE_RAM:
            return "RAM";

        case proto::DmiMemoryDevices::TYPE_ROM:
            return "ROM";

        case proto::DmiMemoryDevices::TYPE_FLASH:
            return "Flash";

        case proto::DmiMemoryDevices::TYPE_EEPROM:
            return "EEPROM";

        case proto::DmiMemoryDevices::TYPE_FEPROM:
            return "FEPROM";

        case proto::DmiMemoryDevices::TYPE_EPROM:
            return "EPROM";

        case proto::DmiMemoryDevices::TYPE_CDRAM:
            return "CDRAM";

        case proto::DmiMemoryDevices::TYPE_3DRAM:
            return "3DRAM";

        case proto::DmiMemoryDevices::TYPE_SDRAM:
            return "SDRAM";

        case proto::DmiMemoryDevices::TYPE_SGRAM:
            return "SGRAM";

        case proto::DmiMemoryDevices::TYPE_RDRAM:
            return "RDRAM";

        case proto::DmiMemoryDevices::TYPE_DDR:
            return "DDR";

        case proto::DmiMemoryDevices::TYPE_DDR2:
            return "DDR2";

        case proto::DmiMemoryDevices::TYPE_DDR2_FB_DIMM:
            return "DDR2 FB-DIMM";

        case proto::DmiMemoryDevices::TYPE_DDR3:
            return "DDR3";

        case proto::DmiMemoryDevices::TYPE_FBD2:
            return "FBD2";

        case proto::DmiMemoryDevices::TYPE_DDR4:
            return "DDR4";

        case proto::DmiMemoryDevices::TYPE_LPDDR:
            return "LPDDR";

        case proto::DmiMemoryDevices::TYPE_LPDDR2:
            return "LPDDR2";

        case proto::DmiMemoryDevices::TYPE_LPDDR3:
            return "LPDDR3";

        case proto::DmiMemoryDevices::TYPE_LPDDR4:
            return "LPDDR4";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiMemoryDevices::FormFactorToString(
    proto::DmiMemoryDevices::FormFactor value)
{
    switch (value)
    {
        case proto::DmiMemoryDevices::FORM_FACTOR_OTHER:
            return "Other";

        case proto::DmiMemoryDevices::FORM_FACTOR_SIMM:
            return "SIMM";

        case proto::DmiMemoryDevices::FORM_FACTOR_SIP:
            return "SIP";

        case proto::DmiMemoryDevices::FORM_FACTOR_CHIP:
            return "Chip";

        case proto::DmiMemoryDevices::FORM_FACTOR_DIP:
            return "DIP";

        case proto::DmiMemoryDevices::FORM_FACTOR_ZIP:
            return "ZIP";

        case proto::DmiMemoryDevices::FORM_FACTOR_PROPRIETARY_CARD:
            return "Proprietary Card";

        case proto::DmiMemoryDevices::FORM_FACTOR_DIMM:
            return "DIMM";

        case proto::DmiMemoryDevices::FORM_FACTOR_TSOP:
            return "TSOP";

        case proto::DmiMemoryDevices::FORM_FACTOR_ROW_OF_CHIPS:
            return "Row Of Chips";

        case proto::DmiMemoryDevices::FORM_FACTOR_RIMM:
            return "RIMM";

        case proto::DmiMemoryDevices::FORM_FACTOR_SODIMM:
            return "SODIMM";

        case proto::DmiMemoryDevices::FORM_FACTOR_SRIMM:
            return "SRIMM";

        case proto::DmiMemoryDevices::FORM_FACTOR_FB_DIMM:
            return "FB-DIMM";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiSystemSlots
//

const char* CategoryDmiSystemSlots::Name() const
{
    return "System Slots";
}

Category::IconId CategoryDmiSystemSlots::Icon() const
{
    return IDI_PORT;
}

const char* CategoryDmiSystemSlots::Guid() const
{
    return "{7A4F71C6-557F-48A5-AC94-E430F69154F1}";
}

void CategoryDmiSystemSlots::Parse(Table& table, const std::string& data)
{
    proto::DmiSystemSlots message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiSystemSlots::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("System Slot #%d", index + 1));

        if (!item.slot_designation().empty())
            group.AddParam("Slot Designation", Value::String(item.slot_designation()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("Usage", Value::String(UsageToString(item.usage())));
        group.AddParam("Bus Width", Value::String(BusWidthToString(item.bus_width())));
        group.AddParam("Length", Value::String(LengthToString(item.length())));
    }
}

std::string CategoryDmiSystemSlots::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiSystemSlots message;

    for (SMBios::TableEnumerator<SMBios::SystemSlotTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::SystemSlotTable table = table_enumerator.GetTable();
        proto::DmiSystemSlots::Item* item = message.add_item();

        item->set_slot_designation(table.GetSlotDesignation());
        item->set_type(table.GetType());
        item->set_usage(table.GetUsage());
        item->set_bus_width(table.GetBusWidth());
        item->set_length(table.GetLength());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiSystemSlots::TypeToString(proto::DmiSystemSlots::Type value)
{
    switch (value)
    {
        case proto::DmiSystemSlots::TYPE_OTHER:
            return "Other";

        case proto::DmiSystemSlots::TYPE_ISA:
            return "ISA";

        case proto::DmiSystemSlots::TYPE_MCA:
            return "MCA";

        case proto::DmiSystemSlots::TYPE_EISA:
            return "EISA";

        case proto::DmiSystemSlots::TYPE_PCI:
            return "PCI";

        case proto::DmiSystemSlots::TYPE_PC_CARD:
            return "PC Card (PCMCIA)";

        case proto::DmiSystemSlots::TYPE_VLB:
            return "VLB";

        case proto::DmiSystemSlots::TYPE_PROPRIETARY:
            return "Proprietary";

        case proto::DmiSystemSlots::TYPE_PROCESSOR_CARD:
            return "Processor Card";

        case proto::DmiSystemSlots::TYPE_PROPRIETARY_MEMORY_CARD:
            return "Proprietary Memory Card";

        case proto::DmiSystemSlots::TYPE_IO_RISER_CARD:
            return "I/O Riser Card";

        case proto::DmiSystemSlots::TYPE_NUBUS:
            return "NuBus";

        case proto::DmiSystemSlots::TYPE_PCI_66:
            return "PCI-66";

        case proto::DmiSystemSlots::TYPE_AGP:
            return "AGP";

        case proto::DmiSystemSlots::TYPE_AGP_2X:
            return "AGP 2x";

        case proto::DmiSystemSlots::TYPE_AGP_4X:
            return "AGP 4x";

        case proto::DmiSystemSlots::TYPE_PCI_X:
            return "PCI-X";

        case proto::DmiSystemSlots::TYPE_AGP_8X:
            return "AGP 8x";

        case proto::DmiSystemSlots::TYPE_M2_SOCKET_1DP:
            return "M.2 Socket 1-DP";

        case proto::DmiSystemSlots::TYPE_M2_SOCKET_1SD:
            return "M.2 Socket 1-SD";

        case proto::DmiSystemSlots::TYPE_M2_SOCKET_2:
            return "M.2 Socket 2";

        case proto::DmiSystemSlots::TYPE_M2_SOCKET_3:
            return "M.2 Socket 3";

        case proto::DmiSystemSlots::TYPE_MXM_TYPE_I:
            return "MXM Type I";

        case proto::DmiSystemSlots::TYPE_MXM_TYPE_II:
            return "MXM Type II";

        case proto::DmiSystemSlots::TYPE_MXM_TYPE_III:
            return "MXM Type III";

        case proto::DmiSystemSlots::TYPE_MXM_TYPE_III_HE:
            return "MXM Type III-HE";

        case proto::DmiSystemSlots::TYPE_MXM_TYPE_IV:
            return "MXM Type IV";

        case proto::DmiSystemSlots::TYPE_MXM_30_TYPE_A:
            return "MXM 3.0 Type A";

        case proto::DmiSystemSlots::TYPE_MXM_30_TYPE_B:
            return "MXM 3.0 Type B";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_SFF_8639:
            return "PCI Express 2 SFF-8639";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_SFF_8639:
            return "PCI Express 3 SFF-8639";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_MINI_52PIN_WITH_BOTTOM_SIDE:
            return "PCI Express Mini 52-pin with bottom-side keep-outs";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_MINI_52PIN:
            return "PCI Express Mini 52-pin without bottom-side keep-outs";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_MINI_76PIN:
            return "PCI Express Mini 76-pin";

        case proto::DmiSystemSlots::TYPE_PC98_C20:
            return "PC-98/C20";

        case proto::DmiSystemSlots::TYPE_PC98_C24:
            return "PC-98/C24";

        case proto::DmiSystemSlots::TYPE_PC98_E:
            return "PC-98/E";

        case proto::DmiSystemSlots::TYPE_PC98_LOCAL_BUS:
            return "PC-98/Local Bus";

        case proto::DmiSystemSlots::TYPE_PC98_CARD:
            return "PC-98/Card";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS:
            return "PCI Express";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X1:
            return "PCI Express x1";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X2:
            return "PCI Express x2";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X4:
            return "PCI Express x4";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X8:
            return "PCI Express x8";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_X16:
            return "PCI Express x16";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2:
            return "PCI Express 2";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X1:
            return "PCI Express 2 x1";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X2:
            return "PCI Express 2 x2";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X4:
            return "PCI Express 2 x4";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X8:
            return "PCI Express 2 x8";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_2_X16:
            return "PCI Express 2 x16";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3:
            return "PCI Express 3";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X1:
            return "PCI Express 3 x1";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X2:
            return "PCI Express 3 x2";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X4:
            return "PCI Express 3 x4";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X8:
            return "PCI Express 3 x8";

        case proto::DmiSystemSlots::TYPE_PCI_EXPRESS_3_X16:
            return "PCI Express 3 x16";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiSystemSlots::UsageToString(proto::DmiSystemSlots::Usage value)
{
    switch (value)
    {
        case proto::DmiSystemSlots::USAGE_OTHER:
            return "Other";

        case proto::DmiSystemSlots::USAGE_AVAILABLE:
            return "Available";

        case proto::DmiSystemSlots::USAGE_IN_USE:
            return "In Use";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiSystemSlots::BusWidthToString(proto::DmiSystemSlots::BusWidth value)
{
    switch (value)
    {
        case proto::DmiSystemSlots::BUS_WIDTH_OTHER:
            return "Other";

        case proto::DmiSystemSlots::BUS_WIDTH_8_BIT:
            return "8-bit";

        case proto::DmiSystemSlots::BUS_WIDTH_16_BIT:
            return "16-bit";

        case proto::DmiSystemSlots::BUS_WIDTH_32_BIT:
            return "32-bit";

        case proto::DmiSystemSlots::BUS_WIDTH_64_BIT:
            return "64-bit";

        case proto::DmiSystemSlots::BUS_WIDTH_128_BIT:
            return "128-bit";

        case proto::DmiSystemSlots::BUS_WIDTH_X1:
            return "x1";

        case proto::DmiSystemSlots::BUS_WIDTH_X2:
            return "x2";

        case proto::DmiSystemSlots::BUS_WIDTH_X4:
            return "x4";

        case proto::DmiSystemSlots::BUS_WIDTH_X8:
            return "x8";

        case proto::DmiSystemSlots::BUS_WIDTH_X12:
            return "x12";

        case proto::DmiSystemSlots::BUS_WIDTH_X16:
            return "x16";

        case proto::DmiSystemSlots::BUS_WIDTH_X32:
            return "x32";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiSystemSlots::LengthToString(proto::DmiSystemSlots::Length value)
{
    switch (value)
    {
        case proto::DmiSystemSlots::LENGTH_OTHER:
            return "Other";

        case proto::DmiSystemSlots::LENGTH_SHORT:
            return "Short";

        case proto::DmiSystemSlots::LENGTH_LONG:
            return "Long";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiPortConnectors
//

const char* CategoryDmiPortConnectors::Name() const
{
    return "Port Connectors";
}

Category::IconId CategoryDmiPortConnectors::Icon() const
{
    return IDI_PORT;
}

const char* CategoryDmiPortConnectors::Guid() const
{
    return "{FF4CE0FE-261F-46EF-852F-42420E68CFD2}";
}

void CategoryDmiPortConnectors::Parse(Table& table, const std::string& data)
{
    proto::DmiPortConnectors message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiPortConnectors::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Port Connector #%d", index + 1));

        if (!item.internal_designation().empty())
            group.AddParam("Internal Designation", Value::String(item.internal_designation()));

        if (!item.external_designation().empty())
            group.AddParam("External Designation", Value::String(item.external_designation()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));

        group.AddParam("Internal Connector Type",
                       Value::String(ConnectorTypeToString(item.internal_connector_type())));
        group.AddParam("External Connector Type",
                       Value::String(ConnectorTypeToString(item.external_connector_type())));
    }
}

std::string CategoryDmiPortConnectors::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiPortConnectors message;

    for (SMBios::TableEnumerator<SMBios::PortConnectorTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::PortConnectorTable table = table_enumerator.GetTable();
        proto::DmiPortConnectors::Item* item = message.add_item();

        item->set_internal_designation(table.GetInternalDesignation());
        item->set_external_designation(table.GetExternalDesignation());
        item->set_type(table.GetType());
        item->set_internal_connector_type(table.GetInternalConnectorType());
        item->set_external_connector_type(table.GetExternalConnectorType());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiPortConnectors::TypeToString(proto::DmiPortConnectors::Type value)
{
    switch (value)
    {
        case proto::DmiPortConnectors::TYPE_NONE:
            return "None";

        case proto::DmiPortConnectors::TYPE_PARALLEL_PORT_XT_AT_COMPATIBLE:
            return "Parallel Port XT/AT Compatible";

        case proto::DmiPortConnectors::TYPE_PARALLEL_PORT_PS_2:
            return "Parallel Port PS/2";

        case proto::DmiPortConnectors::TYPE_PARALLEL_PORT_ECP:
            return "Parallel Port ECP";

        case proto::DmiPortConnectors::TYPE_PARALLEL_PORT_EPP:
            return "Parallel Port EPP";

        case proto::DmiPortConnectors::TYPE_PARALLEL_PORT_ECP_EPP:
            return "Parallel Port ECP/EPP";

        case proto::DmiPortConnectors::TYPE_SERIAL_PORT_XT_AT_COMPATIBLE:
            return "Serial Port XT/AT Compatible";

        case proto::DmiPortConnectors::TYPE_SERIAL_PORT_16450_COMPATIBLE:
            return "Serial Port 16450 Compatible";

        case proto::DmiPortConnectors::TYPE_SERIAL_PORT_16550_COMPATIBLE:
            return "Serial Port 16550 Compatible";

        case proto::DmiPortConnectors::TYPE_SERIAL_PORT_16550A_COMPATIBLE:
            return "Serial Port 16550A Compatible";

        case proto::DmiPortConnectors::TYPE_SCSI_PORT:
            return "SCSI Port";

        case proto::DmiPortConnectors::TYPE_MIDI_PORT:
            return "MIDI Port";

        case proto::DmiPortConnectors::TYPE_JOYSTICK_PORT:
            return "Joystick Port";

        case proto::DmiPortConnectors::TYPE_KEYBOARD_PORT:
            return "Keyboard Port";

        case proto::DmiPortConnectors::TYPE_MOUSE_PORT:
            return "Mouse Port";

        case proto::DmiPortConnectors::TYPE_SSA_SCSI:
            return "SSA SCSI";

        case proto::DmiPortConnectors::TYPE_USB:
            return "USB";

        case proto::DmiPortConnectors::TYPE_FIREWIRE:
            return "Firewire (IEEE 1394)";

        case proto::DmiPortConnectors::TYPE_PCMCIA_TYPE_I:
            return "PCMCIA Type I";

        case proto::DmiPortConnectors::TYPE_PCMCIA_TYPE_II:
            return "PCMCIA Type II";

        case proto::DmiPortConnectors::TYPE_PCMCIA_TYPE_III:
            return "PCMCIA Type III";

        case proto::DmiPortConnectors::TYPE_CARDBUS:
            return "Cardbus";

        case proto::DmiPortConnectors::TYPE_ACCESS_BUS_PORT:
            return "Access Bus Port";

        case proto::DmiPortConnectors::TYPE_SCSI_II:
            return "SCSI II";

        case proto::DmiPortConnectors::TYPE_SCSI_WIDE:
            return "SCSI Wide";

        case proto::DmiPortConnectors::TYPE_PC_98:
            return "PC-98";

        case proto::DmiPortConnectors::TYPE_PC_98_HIRESO:
            return "PC-98 Hireso";

        case proto::DmiPortConnectors::TYPE_PC_H98:
            return "PC-H98";

        case proto::DmiPortConnectors::TYPE_VIDEO_PORT:
            return "Video Port";

        case proto::DmiPortConnectors::TYPE_AUDIO_PORT:
            return "Audio Port";

        case proto::DmiPortConnectors::TYPE_MODEM_PORT:
            return "Modem Port";

        case proto::DmiPortConnectors::TYPE_NETWORK_PORT:
            return "Network Port";

        case proto::DmiPortConnectors::TYPE_SATA:
            return "SATA";

        case proto::DmiPortConnectors::TYPE_SAS:
            return "SAS";

        case proto::DmiPortConnectors::TYPE_8251_COMPATIBLE:
            return "8251 Compatible";

        case proto::DmiPortConnectors::TYPE_8251_FIFO_COMPATIBLE:
            return "8251 FIFO Compatible";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiPortConnectors::ConnectorTypeToString(
    proto::DmiPortConnectors::ConnectorType value)
{
    switch (value)
    {
        case proto::DmiPortConnectors::CONNECTOR_TYPE_NONE:
            return "None";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_OTHER:
            return "Other";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_CENTRONICS:
            return "Centronics";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_CENTRONICS:
            return "Mini Centronics";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_PROPRIETARY:
            return "Proprietary";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_DB_25_MALE:
            return "DB-25 male";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_DB_25_FEMALE:
            return "DB-25 female";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_DB_15_MALE:
            return "DB-15 male";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_DB_15_FEMALE:
            return "DB-15 female";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_DB_9_MALE:
            return "DB-9 male";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_DB_9_FEMALE:
            return "DB-9 female";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_RJ_11:
            return "RJ-11";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_RJ_45:
            return "RJ-45";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_50_PIN_MINISCSI:
            return "50 Pin MiniSCSI";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_DIN:
            return "Mini DIN";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_MICRO_DIN:
            return "Micro DIN";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_PS_2:
            return "PS/2";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_INFRARED:
            return "Infrared";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_HP_HIL:
            return "HP-HIL";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_ACCESS_BUS_USB:
            return "Access Bus (USB)";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_SSA_SCSI:
            return "SSA SCSI";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_CIRCULAR_DIN_8_MALE:
            return "Circular DIN-8 male";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_CIRCULAR_DIN_8_FEMALE:
            return "Circular DIN-8 female";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_ONBOARD_IDE:
            return "On Board IDE";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_ONBOARD_FLOPPY:
            return "On Board Floppy";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_9_PIN_DUAL_INLINE:
            return "9 Pin Dual Inline (pin 10 cut)";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_25_PIN_DUAL_INLINE:
            return "25 Pin Dual Inline (pin 26 cut)";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_50_PIN_DUAL_INLINE:
            return "50 Pin Dual Inline";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_68_PIN_DUAL_INLINE:
            return "68 Pin Dual Inline";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_ONBOARD_SOUND_INPUT_FROM_CDROM:
            return "On Board Sound Input From CD-ROM";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_CENTRONICS_TYPE_14:
            return "Mini Centronics Type-14";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_CENTRONICS_TYPE_26:
            return "Mini Centronics Type-26";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_MINI_JACK:
            return "Mini Jack (headphones)";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_BNC:
            return "BNC";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_IEEE_1394:
            return "IEEE 1394";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_SAS_SATE_PLUG_RECEPTACLE:
            return "SAS/SATA Plug Receptacle";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_PC_98:
            return "PC-98";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_PC_98_HIRESO:
            return "PC-98 Hireso";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_PC_H98:
            return "PC-H98";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_PC_98_NOTE:
            return "PC-98 Note";

        case proto::DmiPortConnectors::CONNECTOR_TYPE_PC_98_FULL:
            return "PC-98 Full";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiOnboardDevices
//

const char* CategoryDmiOnboardDevices::Name() const
{
    return "Onboard Devices";
}

Category::IconId CategoryDmiOnboardDevices::Icon() const
{
    return IDI_MOTHERBOARD;
}

const char* CategoryDmiOnboardDevices::Guid() const
{
    return "{6C62195C-5E5F-41BA-B6AD-99041594DAC6}";
}

void CategoryDmiOnboardDevices::Parse(Table& table, const std::string& data)
{
    proto::DmiOnBoardDevices message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiOnBoardDevices::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("OnBoard Device #%d", index + 1));

        if (!item.description().empty())
            group.AddParam("Description", Value::String(item.description()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("Status", Value::String(item.enabled() ? "Enabled" : "Disabled"));
    }
}

std::string CategoryDmiOnboardDevices::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiOnBoardDevices message;

    for (SMBios::TableEnumerator<SMBios::OnBoardDeviceTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::OnBoardDeviceTable table = table_enumerator.GetTable();

        for (int index = 0; index < table.GetDeviceCount(); ++index)
        {
            proto::DmiOnBoardDevices::Item* item = message.add_item();

            item->set_description(table.GetDescription(index));
            item->set_type(table.GetType(index));
            item->set_enabled(table.IsEnabled(index));
        }
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiOnboardDevices::TypeToString(proto::DmiOnBoardDevices::Type value)
{
    switch (value)
    {
        case proto::DmiOnBoardDevices::TYPE_OTHER:
            return "Other";

        case proto::DmiOnBoardDevices::TYPE_VIDEO:
            return "Video";

        case proto::DmiOnBoardDevices::TYPE_SCSI_CONTROLLER:
            return "SCSI Controller";

        case proto::DmiOnBoardDevices::TYPE_ETHERNET:
            return "Ethernet";

        case proto::DmiOnBoardDevices::TYPE_TOKEN_RING:
            return "Token Ring";

        case proto::DmiOnBoardDevices::TYPE_SOUND:
            return "Sound";

        case proto::DmiOnBoardDevices::TYPE_PATA_CONTROLLER:
            return "PATA Controller";

        case proto::DmiOnBoardDevices::TYPE_SATA_CONTROLLER:
            return "SATA Controller";

        case proto::DmiOnBoardDevices::TYPE_SAS_CONTROLLER:
            return "SAS Controller";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiPointingDevice
//

const char* CategoryDmiPointingDevices::Name() const
{
    return "Pointing Devices";
}

Category::IconId CategoryDmiPointingDevices::Icon() const
{
    return IDI_MOUSE;
}

const char* CategoryDmiPointingDevices::Guid() const
{
    return "{6883684B-3CEC-451B-A2E3-34C16348BA1B}";
}

void CategoryDmiPointingDevices::Parse(Table& table, const std::string& data)
{
    proto::DmiPointingDevices message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiPointingDevices::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Build-in Pointing Device #%d", index + 1));

        group.AddParam("Device Type", Value::String(TypeToString(item.device_type())));
        group.AddParam("Device Interface", Value::String(InterfaceToString(item.device_interface())));
        group.AddParam("Buttons Count", Value::Number(item.button_count()));
    }
}

std::string CategoryDmiPointingDevices::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiPointingDevices message;

    for (SMBios::TableEnumerator<SMBios::PointingDeviceTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::PointingDeviceTable table = table_enumerator.GetTable();
        proto::DmiPointingDevices::Item* item = message.add_item();

        item->set_device_type(table.GetDeviceType());
        item->set_device_interface(table.GetInterface());
        item->set_button_count(table.GetButtonCount());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiPointingDevices::TypeToString(proto::DmiPointingDevices::Type value)
{
    switch (value)
    {
        case proto::DmiPointingDevices::TYPE_OTHER:
            return "Other";

        case proto::DmiPointingDevices::TYPE_MOUSE:
            return "Mouse";

        case proto::DmiPointingDevices::TYPE_TRACK_BALL:
            return "Track Ball";

        case proto::DmiPointingDevices::TYPE_TRACK_POINT:
            return "Track Point";

        case proto::DmiPointingDevices::TYPE_GLIDE_POINT:
            return "Glide Point";

        case proto::DmiPointingDevices::TYPE_TOUCH_PAD:
            return "Touch Pad";

        case proto::DmiPointingDevices::TYPE_TOUCH_SCREEN:
            return "Touch Screen";

        case proto::DmiPointingDevices::TYPE_OPTICAL_SENSOR:
            return "Optical Sensor";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryDmiPointingDevices::InterfaceToString(
    proto::DmiPointingDevices::Interface value)
{
    switch (value)
    {
        case proto::DmiPointingDevices::INTERFACE_OTHER:
            return "Other";

        case proto::DmiPointingDevices::INTERFACE_SERIAL:
            return "Serial";

        case proto::DmiPointingDevices::INTERFACE_PS_2:
            return "PS/2";

        case proto::DmiPointingDevices::INTERFACE_INFRARED:
            return "Infrared";

        case proto::DmiPointingDevices::INTERFACE_HP_HIL:
            return "HP-HIL";

        case proto::DmiPointingDevices::INTERFACE_BUS_MOUSE:
            return "Bus mouse";

        case proto::DmiPointingDevices::INTERFACE_ADB:
            return "ADB (Apple Desktop Bus)";

        case proto::DmiPointingDevices::INTERFACE_BUS_MOUSE_DB_9:
            return "Bus mouse DB-9";

        case proto::DmiPointingDevices::INTERFACE_BUS_MOUSE_MICRO_DIN:
            return "Bus mouse micro-DIN";

        case proto::DmiPointingDevices::INTERFACE_USB:
            return "USB";

        default:
            return "Unknown";
    }
}

//
// CategoryDmiPortableBattery
//

const char* CategoryDmiPortableBattery::Name() const
{
    return "Portable Battery";
}

Category::IconId CategoryDmiPortableBattery::Icon() const
{
    return IDI_BATTERY;
}

const char* CategoryDmiPortableBattery::Guid() const
{
    return "{0CA213B5-12EE-4828-A399-BA65244E65FD}";
}

void CategoryDmiPortableBattery::Parse(Table& table, const std::string& data)
{
    proto::DmiPortableBattery message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiPortableBattery::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Portable Battery #%d", index + 1));

        if (!item.location().empty())
            group.AddParam("Location", Value::String(item.location()));

        if (!item.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(item.manufacturer()));

        if (!item.manufacture_date().empty())
            group.AddParam("Manufacture Date", Value::String(item.manufacture_date()));

        if (!item.sbds_serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.sbds_serial_number()));

        if (!item.device_name().empty())
            group.AddParam("Device Name", Value::String(item.device_name()));

        group.AddParam("Chemistry", Value::String(ChemistryToString(item.chemistry())));

        if (item.design_capacity() != 0)
        {
            group.AddParam("Design Capacity", Value::Number(item.design_capacity(), "mWh"));
        }

        if (item.design_voltage() != 0)
        {
            group.AddParam("Design Voltage", Value::Number(item.design_voltage(), "mV"));
        }

        if (item.max_error_in_battery_data() != 0)
        {
            group.AddParam("Max. Error in Battery Data",
                           Value::Number(item.max_error_in_battery_data(), "%"));
        }

        if (!item.sbds_version_number().empty())
            group.AddParam("SBDS Version Number", Value::String(item.sbds_version_number()));

        if (!item.sbds_serial_number().empty())
            group.AddParam("SBDS Serial Number", Value::String(item.sbds_serial_number()));

        if (!item.sbds_manufacture_date().empty())
            group.AddParam("SBDS Manufacture Date", Value::String(item.sbds_manufacture_date()));

        if (!item.sbds_device_chemistry().empty())
            group.AddParam("SBDS Device Chemistry", Value::String(item.sbds_device_chemistry()));
    }
}

std::string CategoryDmiPortableBattery::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiPortableBattery message;

    for (SMBios::TableEnumerator<SMBios::PortableBatteryTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::PortableBatteryTable table = table_enumerator.GetTable();
        proto::DmiPortableBattery::Item* item = message.add_item();

        item->set_location(table.GetLocation());
        item->set_manufacturer(table.GetManufacturer());
        item->set_manufacture_date(table.GetManufactureDate());
        item->set_serial_number(table.GetSerialNumber());
        item->set_device_name(table.GetDeviceName());
        item->set_chemistry(table.GetChemistry());
        item->set_design_capacity(table.GetDesignCapacity());
        item->set_design_voltage(table.GetDesignVoltage());
        item->set_sbds_version_number(table.GetSBDSVersionNumber());
        item->set_max_error_in_battery_data(table.GetMaxErrorInBatteryData());
        item->set_sbds_serial_number(table.GetSBDSSerialNumber());
        item->set_sbds_manufacture_date(table.GetSBDSManufactureDate());
        item->set_sbds_device_chemistry(table.GetSBDSDeviceChemistry());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryDmiPortableBattery::ChemistryToString(
    proto::DmiPortableBattery::Chemistry value)
{
    switch (value)
    {
        case proto::DmiPortableBattery::CHEMISTRY_OTHER:
            return "Other";

        case proto::DmiPortableBattery::CHEMISTRY_LEAD_ACID:
            return "Lead Acid";

        case proto::DmiPortableBattery::CHEMISTRY_NICKEL_CADMIUM:
            return "Nickel Cadmium";

        case proto::DmiPortableBattery::CHEMISTRY_NICKEL_METAL_HYDRIDE:
            return "Nickel Metal Hydride";

        case proto::DmiPortableBattery::CHEMISTRY_LITHIUM_ION:
            return "Lithium-ion";

        case proto::DmiPortableBattery::CHEMISTRY_ZINC_AIR:
            return "Zinc Air";

        case proto::DmiPortableBattery::CHEMISTRY_LITHIUM_POLYMER:
            return "Lithium Polymer";

        default:
            return "Unknown";
    }
}

//
// CategoryGroupDMI
//

const char* CategoryGroupDMI::Name() const
{
    return "DMI";
}

Category::IconId CategoryGroupDMI::Icon() const
{
    return IDI_COMPUTER;
}

//
// CategoryOpticalDrives
//

const char* CategoryOpticalDrives::Name() const
{
    return "Optical Drives";
}

Category::IconId CategoryOpticalDrives::Icon() const
{
    return IDI_DRIVE_DISK;
}

const char* CategoryOpticalDrives::Guid() const
{
    return "{68E028FE-3DA6-4BAF-9E18-CDB828372860}";
}

void CategoryOpticalDrives::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryOpticalDrives::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupStorage
//

const char* CategoryGroupStorage::Name() const
{
    return "Storage";
}

Category::IconId CategoryGroupStorage::Icon() const
{
    return IDI_DRIVE;
}

//
// CategoryGroupDisplay
//

const char* CategoryGroupDisplay::Name() const
{
    return "Display";
}

Category::IconId CategoryGroupDisplay::Icon() const
{
    return IDI_MONITOR;
}

//
// CategoryGroupHardware
//

const char* CategoryGroupHardware::Name() const
{
    return "Hardware";
}

Category::IconId CategoryGroupHardware::Icon() const
{
    return IDI_HARDWARE;
}

} // namespace aspia
