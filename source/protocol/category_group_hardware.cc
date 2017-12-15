//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_hardware.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/printer_enumerator.h"
#include "base/devices/battery_enumerator.h"
#include "base/devices/monitor_enumerator.h"
#include "base/devices/video_adapter_enumarator.h"
#include "base/devices/physical_drive_enumerator.h"
#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
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
// CategoryCPU
//

const char* CategoryCPU::Name() const
{
    return "Central Processor";
}

Category::IconId CategoryCPU::Icon() const
{
    return IDI_PROCESSOR;
}

const char* CategoryCPU::Guid() const
{
    return "{31D1312E-85A9-419A-91B4-BA81129B3CCC}";
}

void CategoryCPU::Parse(Table& table, const std::string& data)
{
    proto::CPU message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    table.AddParam("Brand String", Value::String(message.brand_string()));
    table.AddParam("Vendor", Value::String(message.vendor()));
    table.AddParam("Stepping", Value::FormattedString("%02Xh", message.stepping()));
    table.AddParam("Model", Value::FormattedString("%02Xh", message.model()));
    table.AddParam("Family", Value::FormattedString("%02Xh", message.family()));

    if (message.extended_model())
    {
        table.AddParam("Extended Model", Value::FormattedString("%02Xh", message.extended_model()));
    }

    if (message.extended_family())
    {
        table.AddParam("Extended Family", Value::FormattedString("%02Xh", message.extended_family()));
    }

    table.AddParam("Brand ID", Value::FormattedString("%02Xh", message.brand_id()));
    table.AddParam("Packages", Value::Number(message.packages()));
    table.AddParam("Physical Cores", Value::Number(message.physical_cores()));
    table.AddParam("Logical Cores", Value::Number(message.logical_cores()));

    if (message.has_features())
    {
        std::vector<std::pair<std::string, bool>> list;

        auto add = [&](bool is_supported, const char* name)
        {
            list.emplace_back(name, is_supported);
        };

        const proto::CPU::Features& features = message.features();

        add(features.has_fpu(), "Floating-point Unit On-Chip (FPU)");
        add(features.has_vme(), "Virtual Mode Extension (VME)");
        add(features.has_de(), "Debug Extension (DE)");
        add(features.has_pse(), "Page Size Extension (PSE)");
        add(features.has_tsc(), "Time Stamp Extension (TSC)");
        add(features.has_msr(), "Model Specific Registers (MSR)");
        add(features.has_pae(), "Physical Address Extension (PAE)");
        add(features.has_mce(), "Machine-Check Exception (MCE)");
        add(features.has_cx8(), "CMPXCHG8 Instruction (CX8)");
        add(features.has_apic(), "On-cpip APIC Hardware (APIC)");
        add(features.has_sep(), "Fast System Call (SEP)");
        add(features.has_mtrr(), "Memory Type Range Registers (MTRR)");
        add(features.has_pge(), "Page Global Enable (PGE)");
        add(features.has_mca(), "Machine-Check Architecture (MCA)");
        add(features.has_cmov(), "Conditional Move Instruction (CMOV)");
        add(features.has_pat(), "Page Attribute Table (PAT)");
        add(features.has_pse36(), "36-bit Page Size Extension (PSE36)");
        add(features.has_psn(), "Processor Serial Number (PSN)");
        add(features.has_clfsh(), "CLFLUSH Instruction");
        add(features.has_ds(), "Debug Store (DS)");
        add(features.has_acpu(), "Thermal Monitor and Software Controlled Clock Facilities (ACPU)");
        add(features.has_mmx(), "MMX Technology (MMX)");
        add(features.has_fxsr(), "FXSAVE and FXSTOR Instructions (FXSR)");
        add(features.has_sse(), "Streaming SIMD Extension (SSE)");
        add(features.has_sse2(), "Streaming SIMD Extension 2 (SSE2)");
        add(features.has_ss(), "Self-Snoop (SS)");
        add(features.has_htt(), "Multi-Threading (HTT)");
        add(features.has_tm(), "Thermal Monitor (TM)");
        add(features.has_ia64(), "IA64 Processor Emulating x86");
        add(features.has_pbe(), "Pending Break Enable (PBE)");
        add(features.has_sse3(), "Streaming SIMD Extension 3 (SSE3)");
        add(features.has_pclmuldq(), "PCLMULDQ Instruction");
        add(features.has_dtes64(), "64-Bit Debug Store (DTES64)");
        add(features.has_monitor(), "MONITOR/MWAIT Instructions");
        add(features.has_ds_cpl(), "CPL Qualified Debug Store");
        add(features.has_vmx(), "Virtual Machine Extensions (VMX, Vanderpool)");
        add(features.has_smx(), "Safe Mode Extensions (SMX)");
        add(features.has_est(), "Enhanced SpeedStep Technology (EIST, ESS)");
        add(features.has_tm2(), "Thermal Monitor 2 (TM2)");
        add(features.has_ssse3(), "Supplemental Streaming SIMD Extension 3 (SSSE3)");
        add(features.has_cnxt_id(), "L1 Context ID");
        add(features.has_sdbg(), "Silicon Debug Interface");
        add(features.has_fma(), "Fused Multiply Add (FMA)");
        add(features.has_cx16(), "CMPXCHG16B Instruction");
        add(features.has_xtpr(), "xTPR Update Control");
        add(features.has_pdcm(), "Perfmon and Debug Capability");
        add(features.has_pcid(), "Process Context Identifiers");
        add(features.has_dca(), "Direct Cache Access (DCA)");
        add(features.has_sse41(), "Streaming SIMD Extension 4.1 (SSE4.1)");
        add(features.has_sse42(), "Streaming SIMD Extension 4.2 (SSE4.2)");
        add(features.has_x2apic(), "Extended xAPIC Support");
        add(features.has_movbe(), "MOVBE Instruction");
        add(features.has_popcnt(), "POPCNT Instruction");
        add(features.has_tsc_deadline(), "Time Stamp Counter Deadline");
        add(features.has_aes(), "AES Instruction (AES)");
        add(features.has_xsave(), "XSAVE/XSTOR States");
        add(features.has_osxsave(), "OS-Enabled Extended State Management");
        add(features.has_avx(), "Advanced Vector Extension (AVX)");
        add(features.has_f16c(), "Float-16-bit Convertsion Instructions (F16C)");
        add(features.has_rdrand(), "RDRAND Instruction");
        add(features.has_hypervisor(), "Hypervisor");
        add(features.has_syscall(), "SYSCALL / SYSRET Instructions");
        add(features.has_xd_bit(), "Execution Disable Bit");
        add(features.has_mmxext(), "AMD Extended MMX");
        add(features.has_1gb_pages(), "1 GB Page Size");
        add(features.has_rdtscp(), "RDTSCP Instruction");
        add(features.has_intel64(), "64-bit x86 Extension (AMD64, Intel64)");
        add(features.has_3dnowext(), "AMD Extended 3DNow!");
        add(features.has_3dnow(), "AMD 3DNow!");

        add(features.has_lahf(), "LAHF/SAHF Instructions");
        add(features.has_svm(), "Secure Virtual Machine (SVM, Pacifica)");
        add(features.has_extapic(), "Extended APIC Register Space");
        add(features.has_lzcnt(), "LZCNT Instruction");
        add(features.has_sse4a(), "AMD SSE4A");
        add(features.has_misalignsse(), "AMD MisAligned SSE");
        add(features.has_3dnow_prefetch(), "AMD 3DNowPrefetch");
        add(features.has_ibs(), "Instruction Based Sampling");
        add(features.has_xop(), "AMD XOP");
        add(features.has_skinit(), "SKINIT / STGI Instruction");
        add(features.has_wdt(), "Watchdog Timer");
        add(features.has_lwp(), "Light Weight Profiling");
        add(features.has_fma4(), "AMD FMA4");
        add(features.has_tbm(), "Trailing Bit Manipulation Instructions");
        add(features.has_perfctr_core(), "Core Performance Counters");

        add(features.has_fsgsbase(), "RDFSBASE / RDGSBASE / WRFSBASE / WRGSBASE Instruction");
        add(features.has_sgx(), "Software Guard Extensions (SGE)");
        add(features.has_bmi1(), "Bit Manipulation Instruction Set 1 (BMI1)");
        add(features.has_hle(), "Transactional Synchronization Extensions");
        add(features.has_avx2(), "Advanced Vector Extensions 2 (AVX2)");
        add(features.has_smep(), "Supervisor-Mode Execution Prevention");
        add(features.has_bmi2(), "Bit Manipulation Instruction Set 2 (BMI2)");
        add(features.has_erms(), "Enhanced REP MOVSB/STOSB");
        add(features.has_invpcid(), "INVPCID Instruction");
        add(features.has_rtm(), "Transactional Synchronization Extensions (RTM)");
        add(features.has_pqm(), "Platform Quality of Service Monitoring (PQM)");
        add(features.has_mpx(), "Memory Protection Extensions (MPX)");
        add(features.has_pqe(), "Platform Quality of Service Enforcement (PQE)");
        add(features.has_avx512f(), "AVX-512 Foundation (AVX512F)");
        add(features.has_avx512dq(), "AVX-512 Doubleword and Quadword Instructions (AVX512DQ)");
        add(features.has_rdseed(), "RDSEED Instruction");
        add(features.has_adx(), "Multi-Precision Add-Carry Instruction Extensions (ADX)");
        add(features.has_smap(), "Supervisor Mode Access Prevention (SMAP)");
        add(features.has_avx512ifma(), "AVX-512 Integer Fused Multiply-Add Instructions");
        add(features.has_pcommit(), "PCOMMIT Instruction");
        add(features.has_clflushopt(), "CLFLUSHOPT Instruction");
        add(features.has_clwb(), "CLWB Instruction");
        add(features.has_intel_pt(), "Intel Processor Trace");
        add(features.has_avx512pf(), "AVX-512 Prefetch Instructions (AVX512PF)");
        add(features.has_avx512er(), "AVX-512 Exponential and Reciprocal Instructions (AVX512ER)");
        add(features.has_avx512cd(), "AVX-512 Conflict Detection Instructions (AVX512CD)");
        add(features.has_sha(), "Intel SHA Extensions (SHA)");
        add(features.has_avx512bw(), "AVX-512 Byte and Word Instructions (AVX512BW)");
        add(features.has_avx512vl(), "AVX-512 Vector Length Extensions (AVX512VL)");
        add(features.has_prefetchwt1(), "PREFETCHWT1 Instruction");
        add(features.has_avx512vbmi(), "AVX-512 Vector Bit Manipulation Instructions (AVX512VBMI)");
        add(features.has_umip(), "User-mode Instruction Prevention (UMIP)");
        add(features.has_pku(), "Memory Protection Keys for User-mode pages (PKU)");
        add(features.has_ospke(), "PKU enabled by OS (OSPKE)");
        add(features.has_avx512vbmi2(), "AVX-512 Vector Bit Manipulation Instructions 2 (AVX512VBMI2)");
        add(features.has_gfni(), "Galois Field Instructions (GFNI)");
        add(features.has_vaes(), "AES Instruction Set (VEX-256/EVEX)");
        add(features.has_vpclmulqdq(), "CLMUL Instruction Set (VEX-256/EVEX)");
        add(features.has_avx512vnni(), "AVX-512 Vector Neural Network Instructions (AVX512VNNI)");
        add(features.has_avx512bitalg(), "AVX-512 BITALG Instructions (AVX512BITALG)");
        add(features.has_avx512vpopcntdq(), "AVX-512 Vector Population Count D/Q (AVX512VPOPCNTDQ)");
        add(features.has_rdpid(), "Read Processor ID (RDPID)");
        add(features.has_sgx_lc(), "SGX Launch Configuration");
        add(features.has_avx512_4vnniw(), "AVX-512 4-register Neural Network Instructions (AVX5124VNNIW)");
        add(features.has_avx512_4fmaps(), "AVX-512 4-register Multiply Accumulation Single precision (AVX5124FMAPS)");
        add(features.has_ais(), "VIA Alternate Instruction Set");
        add(features.has_rng(), "Hardware Random Number Generator (RNG)");
        add(features.has_lh(), "LongHaul MSR 0000_110Ah");
        add(features.has_femms(), "VIA FEMMS Instruction");
        add(features.has_ace(), "Advanced Cryptography Engine (ACE)");
        add(features.has_ace2(), "Advanced Cryptography Engine 2 (ACE2)");
        add(features.has_phe(), "PadLock Hash Engine (PHE)");
        add(features.has_pmm(), "PadLock Montgomery Multiplier (PMM)");
        add(features.has_parallax(), "Parallax");
        add(features.has_overstress(), "Overstress");
        add(features.has_tm3(), "Thermal Monitor 3 (TM3)");
        add(features.has_rng2(), "Hardware Random Number Generator 2 (RNG2)");
        add(features.has_phe2(), "PadLock Hash Engine 2 (PHE2)");

        std::sort(list.begin(), list.end());

        Group group = table.AddGroup("Features");

        for (const auto& list_item : list)
        {
            group.AddParam(list_item.first, Value::Bool(list_item.second));
        }
    }
}

std::string CategoryCPU::Serialize()
{
    proto::CPU message;

    CPUInfo cpu;
    GetCPUInformation(cpu);

    message.set_brand_string(CollapseWhitespaceASCII(cpu.brand_string, true));
    message.set_vendor(CollapseWhitespaceASCII(cpu.vendor, true));
    message.set_stepping(cpu.stepping);
    message.set_model(cpu.model);
    message.set_family(cpu.family);
    message.set_extended_model(cpu.extended_model);
    message.set_extended_family(cpu.extended_family);
    message.set_brand_id(cpu.brand_id);
    message.set_packages(cpu.package_count);
    message.set_physical_cores(cpu.physical_core_count);
    message.set_logical_cores(cpu.logical_core_count);

    proto::CPU::Features* features = message.mutable_features();

    // Function 1 EDX
    features->set_has_fpu(cpu.fn_1_edx.has_fpu);
    features->set_has_vme(cpu.fn_1_edx.has_vme);
    features->set_has_de(cpu.fn_1_edx.has_de);
    features->set_has_pse(cpu.fn_1_edx.has_pse);
    features->set_has_tsc(cpu.fn_1_edx.has_tsc);
    features->set_has_msr(cpu.fn_1_edx.has_msr);
    features->set_has_pae(cpu.fn_1_edx.has_pae);
    features->set_has_mce(cpu.fn_1_edx.has_mce);
    features->set_has_cx8(cpu.fn_1_edx.has_cx8);
    features->set_has_apic(cpu.fn_1_edx.has_apic);
    features->set_has_sep(cpu.fn_1_edx.has_sep);
    features->set_has_mtrr(cpu.fn_1_edx.has_mtrr);
    features->set_has_pge(cpu.fn_1_edx.has_pge);
    features->set_has_mca(cpu.fn_1_edx.has_mca);
    features->set_has_cmov(cpu.fn_1_edx.has_cmov);
    features->set_has_pat(cpu.fn_1_edx.has_pat);
    features->set_has_pse36(cpu.fn_1_edx.has_pse36);
    features->set_has_psn(cpu.fn_1_edx.has_psn);
    features->set_has_clfsh(cpu.fn_1_edx.has_clfsh);
    features->set_has_ds(cpu.fn_1_edx.has_ds);
    features->set_has_acpu(cpu.fn_1_edx.has_acpu);
    features->set_has_mmx(cpu.fn_1_edx.has_mmx);
    features->set_has_fxsr(cpu.fn_1_edx.has_fxsr);
    features->set_has_sse(cpu.fn_1_edx.has_sse);
    features->set_has_sse2(cpu.fn_1_edx.has_sse2);
    features->set_has_ss(cpu.fn_1_edx.has_ss);
    features->set_has_htt(cpu.fn_1_edx.has_htt);
    features->set_has_tm(cpu.fn_1_edx.has_tm);
    features->set_has_ia64(cpu.fn_1_edx.has_ia64);
    features->set_has_pbe(cpu.fn_1_edx.has_pbe);

    // Function 1 ECX
    features->set_has_sse3(cpu.fn_1_ecx.has_sse3);
    features->set_has_pclmuldq(cpu.fn_1_ecx.has_pclmuldq);
    features->set_has_dtes64(cpu.fn_1_ecx.has_dtes64);
    features->set_has_monitor(cpu.fn_1_ecx.has_monitor);
    features->set_has_ds_cpl(cpu.fn_1_ecx.has_ds_cpl);
    features->set_has_vmx(cpu.fn_1_ecx.has_vmx);
    features->set_has_smx(cpu.fn_1_ecx.has_smx);
    features->set_has_est(cpu.fn_1_ecx.has_est);
    features->set_has_tm2(cpu.fn_1_ecx.has_tm2);
    features->set_has_ssse3(cpu.fn_1_ecx.has_ssse3);
    features->set_has_cnxt_id(cpu.fn_1_ecx.has_cnxt_id);
    features->set_has_sdbg(cpu.fn_1_ecx.has_sdbg);
    features->set_has_fma(cpu.fn_1_ecx.has_fma);
    features->set_has_cx16(cpu.fn_1_ecx.has_cx16);
    features->set_has_xtpr(cpu.fn_1_ecx.has_xtpr);
    features->set_has_pdcm(cpu.fn_1_ecx.has_pdcm);
    features->set_has_pcid(cpu.fn_1_ecx.has_pcid);
    features->set_has_dca(cpu.fn_1_ecx.has_dca);
    features->set_has_sse41(cpu.fn_1_ecx.has_sse41);
    features->set_has_sse42(cpu.fn_1_ecx.has_sse42);
    features->set_has_x2apic(cpu.fn_1_ecx.has_x2apic);
    features->set_has_movbe(cpu.fn_1_ecx.has_movbe);
    features->set_has_popcnt(cpu.fn_1_ecx.has_popcnt);
    features->set_has_tsc_deadline(cpu.fn_1_ecx.has_tsc_deadline);
    features->set_has_aes(cpu.fn_1_ecx.has_aes);
    features->set_has_xsave(cpu.fn_1_ecx.has_xsave);
    features->set_has_osxsave(cpu.fn_1_ecx.has_osxsave);
    features->set_has_avx(cpu.fn_1_ecx.has_avx);
    features->set_has_f16c(cpu.fn_1_ecx.has_f16c);
    features->set_has_rdrand(cpu.fn_1_ecx.has_rdrand);
    features->set_has_hypervisor(cpu.fn_1_ecx.has_hypervisor);

    // Function 0x80000001 EDX
    features->set_has_syscall(cpu.fn_81_edx.has_syscall);
    features->set_has_xd_bit(cpu.fn_81_edx.has_xd_bit);
    features->set_has_mmxext(cpu.fn_81_edx.has_mmxext);
    features->set_has_1gb_pages(cpu.fn_81_edx.has_1gb_pages);
    features->set_has_rdtscp(cpu.fn_81_edx.has_rdtscp);
    features->set_has_intel64(cpu.fn_81_edx.has_intel64);
    features->set_has_3dnowext(cpu.fn_81_edx.has_3dnowext);
    features->set_has_3dnow(cpu.fn_81_edx.has_3dnow);

    // Function 0x80000001 ECX
    features->set_has_lahf(cpu.fn_81_ecx.has_lahf);
    features->set_has_svm(cpu.fn_81_ecx.has_svm);
    features->set_has_extapic(cpu.fn_81_ecx.has_extapic);
    features->set_has_lzcnt(cpu.fn_81_ecx.has_lzcnt);
    features->set_has_sse4a(cpu.fn_81_ecx.has_sse4a);
    features->set_has_misalignsse(cpu.fn_81_ecx.has_misalignsse);
    features->set_has_3dnow_prefetch(cpu.fn_81_ecx.has_3dnow_prefetch);
    features->set_has_ibs(cpu.fn_81_ecx.has_ibs);
    features->set_has_xop(cpu.fn_81_ecx.has_xop);
    features->set_has_skinit(cpu.fn_81_ecx.has_skinit);
    features->set_has_wdt(cpu.fn_81_ecx.has_wdt);
    features->set_has_lwp(cpu.fn_81_ecx.has_lwp);
    features->set_has_fma4(cpu.fn_81_ecx.has_fma4);
    features->set_has_tbm(cpu.fn_81_ecx.has_tbm);
    features->set_has_perfctr_core(cpu.fn_81_ecx.has_perfctr_core);

    // Function 7 EBX
    features->set_has_fsgsbase(cpu.fn_7_0_ebx.has_fsgsbase);
    features->set_has_sgx(cpu.fn_7_0_ebx.has_sgx);
    features->set_has_bmi1(cpu.fn_7_0_ebx.has_bmi1);
    features->set_has_hle(cpu.fn_7_0_ebx.has_hle);
    features->set_has_avx2(cpu.fn_7_0_ebx.has_avx2);
    features->set_has_smep(cpu.fn_7_0_ebx.has_smep);
    features->set_has_bmi2(cpu.fn_7_0_ebx.has_bmi2);
    features->set_has_erms(cpu.fn_7_0_ebx.has_erms);
    features->set_has_invpcid(cpu.fn_7_0_ebx.has_invpcid);
    features->set_has_rtm(cpu.fn_7_0_ebx.has_rtm);
    features->set_has_pqm(cpu.fn_7_0_ebx.has_pqm);
    features->set_has_mpx(cpu.fn_7_0_ebx.has_mpx);
    features->set_has_pqe(cpu.fn_7_0_ebx.has_pqe);
    features->set_has_avx512f(cpu.fn_7_0_ebx.has_avx512f);
    features->set_has_avx512dq(cpu.fn_7_0_ebx.has_avx512dq);
    features->set_has_rdseed(cpu.fn_7_0_ebx.has_rdseed);
    features->set_has_adx(cpu.fn_7_0_ebx.has_adx);
    features->set_has_smap(cpu.fn_7_0_ebx.has_smap);
    features->set_has_avx512ifma(cpu.fn_7_0_ebx.has_avx512ifma);
    features->set_has_pcommit(cpu.fn_7_0_ebx.has_pcommit);
    features->set_has_clflushopt(cpu.fn_7_0_ebx.has_clflushopt);
    features->set_has_clwb(cpu.fn_7_0_ebx.has_clwb);
    features->set_has_intel_pt(cpu.fn_7_0_ebx.has_intel_pt);
    features->set_has_avx512pf(cpu.fn_7_0_ebx.has_avx512pf);
    features->set_has_avx512er(cpu.fn_7_0_ebx.has_avx512er);
    features->set_has_avx512cd(cpu.fn_7_0_ebx.has_avx512cd);
    features->set_has_sha(cpu.fn_7_0_ebx.has_sha);
    features->set_has_avx512bw(cpu.fn_7_0_ebx.has_avx512bw);
    features->set_has_avx512vl(cpu.fn_7_0_ebx.has_avx512vl);

    // Function 7 ECX
    features->set_has_prefetchwt1(cpu.fn_7_0_ecx.has_prefetchwt1);
    features->set_has_avx512vbmi(cpu.fn_7_0_ecx.has_avx512vbmi);
    features->set_has_umip(cpu.fn_7_0_ecx.has_umip);
    features->set_has_pku(cpu.fn_7_0_ecx.has_pku);
    features->set_has_ospke(cpu.fn_7_0_ecx.has_ospke);
    features->set_has_avx512vbmi2(cpu.fn_7_0_ecx.has_avx512vbmi2);
    features->set_has_gfni(cpu.fn_7_0_ecx.has_gfni);
    features->set_has_vaes(cpu.fn_7_0_ecx.has_vaes);
    features->set_has_vpclmulqdq(cpu.fn_7_0_ecx.has_vpclmulqdq);
    features->set_has_avx512vnni(cpu.fn_7_0_ecx.has_avx512vnni);
    features->set_has_avx512bitalg(cpu.fn_7_0_ecx.has_avx512bitalg);
    features->set_has_avx512vpopcntdq(cpu.fn_7_0_ecx.has_avx512vpopcntdq);
    features->set_has_rdpid(cpu.fn_7_0_ecx.has_rdpid);
    features->set_has_sgx_lc(cpu.fn_7_0_ecx.has_sgx_lc);

    // Function 7 EDX
    features->set_has_avx512_4vnniw(cpu.fn_7_0_edx.has_avx512_4vnniw);
    features->set_has_avx512_4fmaps(cpu.fn_7_0_edx.has_avx512_4fmaps);

    // Function 0xC0000001 EDX
    features->set_has_ais(cpu.fn_c1_edx.has_ais);
    features->set_has_rng(cpu.fn_c1_edx.has_rng);
    features->set_has_lh(cpu.fn_c1_edx.has_lh);
    features->set_has_femms(cpu.fn_c1_edx.has_femms);
    features->set_has_ace(cpu.fn_c1_edx.has_ace);
    features->set_has_ace2(cpu.fn_c1_edx.has_ace2);
    features->set_has_phe(cpu.fn_c1_edx.has_phe);
    features->set_has_pmm(cpu.fn_c1_edx.has_pmm);
    features->set_has_parallax(cpu.fn_c1_edx.has_parallax);
    features->set_has_overstress(cpu.fn_c1_edx.has_overstress);
    features->set_has_tm3(cpu.fn_c1_edx.has_tm3);
    features->set_has_rng2(cpu.fn_c1_edx.has_rng2);
    features->set_has_phe2(cpu.fn_c1_edx.has_phe2);

    return message.SerializeAsString();
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
// CategoryATA
//

const char* CategoryATA::Name() const
{
    return "ATA";
}

Category::IconId CategoryATA::Icon() const
{
    return IDI_DRIVE;
}

const char* CategoryATA::Guid() const
{
    return "{79D80586-D264-46E6-8718-09E267730B78}";
}

void CategoryATA::Parse(Table& table, const std::string& data)
{
    proto::AtaDrives message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::AtaDrives::Item& item = message.item(index);

        Group group = table.AddGroup(item.model_number());

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.firmware_revision().empty())
            group.AddParam("Firmware Revision", Value::String(item.firmware_revision()));

        group.AddParam("Bus Type", Value::String(BusTypeToString(item.bus_type())));
        group.AddParam("Transfer Mode", Value::String(TransferModeToString(item.transfer_mode())));

        if (item.rotation_rate())
            group.AddParam("Rotation Rate", Value::Number(item.rotation_rate(), "RPM"));

        if (item.drive_size())
            group.AddParam("Drive Size", Value::Number(item.drive_size(), "Bytes"));

        if (item.buffer_size())
            group.AddParam("Buffer Size", Value::Number(item.buffer_size(), "Bytes"));

        if (item.multisectors())
            group.AddParam("Multisectors", Value::Number(item.multisectors()));

        if (item.ecc_size())
            group.AddParam("ECC Size", Value::Number(item.ecc_size()));

        group.AddParam("Removable", Value::Bool(item.is_removable()));

        if (item.heads_number())
            group.AddParam("Heads Count", Value::Number(item.heads_number()));

        if (item.cylinders_number())
            group.AddParam("Cylinders Count", Value::Number(item.cylinders_number()));

        if (item.tracks_per_cylinder())
            group.AddParam("Tracks per Cylinder", Value::Number(item.tracks_per_cylinder()));

        if (item.sectors_per_track())
            group.AddParam("Sectors per Track", Value::Number(item.sectors_per_track()));

        if (item.bytes_per_sector())
            group.AddParam("Bytes per Sector", Value::Number(item.bytes_per_sector()));

        if (item.supported_features())
        {
            Group features_group = group.AddGroup("Features");

            auto add_feature = [&](const char* name, uint64_t flag)
            {
                if (item.supported_features() & flag)
                    features_group.AddParam(name, Value::Bool(item.enabled_features() & flag));
            };

            add_feature("48-bit LBA", proto::AtaDrives::FEATURE_48BIT_LBA);
            add_feature("Advanced Power Management", proto::AtaDrives::FEATURE_ADVANCED_POWER_MANAGEMENT);
            add_feature("Automatic Acoustic Management", proto::AtaDrives::FEATURE_AUTOMATIC_ACOUSTIC_MANAGEMENT);
            add_feature("Device Configuration Overlay", proto::AtaDrives::FEATURE_DEVICE_CONFIGURATION_OVERLAY);
            add_feature("General Purpose Logging", proto::AtaDrives::FEATURE_GENERAL_PURPOSE_LOGGING);
            add_feature("Host Protected Area", proto::AtaDrives::FEATURE_HOST_PROTECTED_AREA);
            add_feature("Read Lock Ahead", proto::AtaDrives::FEATURE_READ_LOCK_AHEAD);
            add_feature("Write Cache", proto::AtaDrives::FEATURE_WRITE_CACHE);
            add_feature("Native Command Queuing", proto::AtaDrives::FEATURE_NATIVE_COMMAND_QUEUING);
            add_feature("Power Management", proto::AtaDrives::FEATURE_POWER_MANAGEMENT);
            add_feature("Power Up In Standby", proto::AtaDrives::FEATURE_POWER_UP_IN_STANDBY);
            add_feature("Release Interrupt", proto::AtaDrives::FEATURE_RELEASE_INTERRUPT);
            add_feature("Service Interrupt", proto::AtaDrives::FEATURE_SERVICE_INTERRUPT);
            add_feature("Security Mode", proto::AtaDrives::FEATURE_SECURITY_MODE);
            add_feature("Streaming", proto::AtaDrives::FEATURE_STREAMING);
            add_feature("SMART", proto::AtaDrives::FEATURE_SMART);
            add_feature("SMART Error Logging", proto::AtaDrives::FEATURE_SMART_ERROR_LOGGING);
            add_feature("SMART Self Test", proto::AtaDrives::FEATURE_SMART_SELF_TEST);
            add_feature("TRIM", proto::AtaDrives::FEATURE_TRIM);
        }
    }
}

std::string CategoryATA::Serialize()
{
    proto::AtaDrives message;

    for (PhysicalDriveEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::AtaDrives::Item* item = message.add_item();

        item->set_model_number(enumerator.GetModelNumber());
        item->set_serial_number(enumerator.GetSerialNumber());
        item->set_firmware_revision(enumerator.GetFirmwareRevision());
        item->set_bus_type(enumerator.GetBusType());
        item->set_transfer_mode(enumerator.GetCurrentTransferMode());
        item->set_rotation_rate(enumerator.GetRotationRate());
        item->set_drive_size(enumerator.GetDriveSize());
        item->set_buffer_size(enumerator.GetBufferSize());
        item->set_multisectors(enumerator.GetMultisectors());
        item->set_ecc_size(enumerator.GetECCSize());
        item->set_is_removable(enumerator.IsRemovable());
        item->set_cylinders_number(enumerator.GetCylindersNumber());
        item->set_tracks_per_cylinder(enumerator.GetTracksPerCylinder());
        item->set_sectors_per_track(enumerator.GetSectorsPerTrack());
        item->set_bytes_per_sector(enumerator.GetBytesPerSector());
        item->set_heads_number(enumerator.GetHeadsNumber());
        item->set_supported_features(enumerator.GetSupportedFeatures());
        item->set_enabled_features(enumerator.GetEnabledFeatures());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryATA::BusTypeToString(proto::AtaDrives::BusType value)
{
    switch (value)
    {
        case proto::AtaDrives::BUS_TYPE_SCSI:
            return "SCSI";

        case proto::AtaDrives::BUS_TYPE_ATAPI:
            return "ATAPI";

        case proto::AtaDrives::BUS_TYPE_ATA:
            return "ATA";

        case proto::AtaDrives::BUS_TYPE_IEEE1394:
            return "IEEE 1394";

        case proto::AtaDrives::BUS_TYPE_SSA:
            return "SSA";

        case proto::AtaDrives::BUS_TYPE_FIBRE:
            return "Fibre";

        case proto::AtaDrives::BUS_TYPE_USB:
            return "USB";

        case proto::AtaDrives::BUS_TYPE_RAID:
            return "RAID";

        case proto::AtaDrives::BUS_TYPE_ISCSI:
            return "iSCSI";

        case proto::AtaDrives::BUS_TYPE_SAS:
            return "SAS";

        case proto::AtaDrives::BUS_TYPE_SATA:
            return "SATA";

        case proto::AtaDrives::BUS_TYPE_SD:
            return "SD";

        case proto::AtaDrives::BUS_TYPE_MMC:
            return "MMC";

        case proto::AtaDrives::BUS_TYPE_VIRTUAL:
            return "Virtual";

        case proto::AtaDrives::BUS_TYPE_FILE_BACKED_VIRTUAL:
            return "File Backed Virtual";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryATA::TransferModeToString(proto::AtaDrives::TransferMode value)
{
    switch (value)
    {
        case proto::AtaDrives::TRANSFER_MODE_PIO:
            return "PIO";

        case proto::AtaDrives::TRANSFER_MODE_PIO_DMA:
            return "PIO / DMA";

        case proto::AtaDrives::TRANSFER_MODE_ULTRA_DMA_133:
            return "UltraDMA/133 (133 MB/s)";

        case proto::AtaDrives::TRANSFER_MODE_ULTRA_DMA_100:
            return "UltraDMA/100 (100 MB/s)";

        case proto::AtaDrives::TRANSFER_MODE_ULTRA_DMA_66:
            return "UltraDMA/66 (66 MB/s)";

        case proto::AtaDrives::TRANSFER_MODE_ULTRA_DMA_44:
            return "UltraDMA/44 (44 MB/s)";

        case proto::AtaDrives::TRANSFER_MODE_ULTRA_DMA_33:
            return "UltraDMA/33 (33 MB/s)";

        case proto::AtaDrives::TRANSFER_MODE_ULTRA_DMA_25:
            return "UltraDMA/25 (25 MB/s)";

        case proto::AtaDrives::TRANSFER_MODE_ULTRA_DMA_16:
            return "UltraDMA/16 (16 MB/s)";

        case proto::AtaDrives::TRANSFER_MODE_SATA_600:
            return "SATA/600 (600 MB/s)";

        case proto::AtaDrives::TRANSFER_MODE_SATA_300:
            return "SATA/300 (300 MB/s)";

        case proto::AtaDrives::TRANSFER_MODE_SATA_150:
            return "SATA/150 (150 MB/s)";

        default:
            return "Unknown";
    }
}

//
// CategorySMART
//

const char* CategorySMART::Name() const
{
    return "S.M.A.R.T.";
}

Category::IconId CategorySMART::Icon() const
{
    return IDI_DRIVE;
}

const char* CategorySMART::Guid() const
{
    return "{7B1F2ED7-7A2E-4F5C-A70B-A56AB5B8CE00}";
}

void CategorySMART::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategorySMART::Serialize()
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
// CategoryVideoAdapters
//

const char* CategoryVideoAdapters::Name() const
{
    return "Video Adapters";
}

Category::IconId CategoryVideoAdapters::Icon() const
{
    return IDI_MONITOR;
}

const char* CategoryVideoAdapters::Guid() const
{
    return "{09E9069D-C394-4CD7-8252-E5CF83B7674C}";
}

void CategoryVideoAdapters::Parse(Table& table, const std::string& data)
{
    proto::VideoAdapters message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::VideoAdapters::Item& item = message.item(index);

        Group group = table.AddGroup(item.description());

        if (!item.description().empty())
            group.AddParam("Description", Value::String(item.description()));

        if (!item.adapter_string().empty())
            group.AddParam("Adapter String", Value::String(item.adapter_string()));

        if (!item.bios_string().empty())
            group.AddParam("BIOS String", Value::String(item.bios_string()));

        if (!item.chip_type().empty())
            group.AddParam("Chip Type", Value::String(item.chip_type()));

        if (!item.dac_type().empty())
            group.AddParam("DAC Type", Value::String(item.dac_type()));

        if (item.memory_size())
            group.AddParam("Memory Size", Value::Number(item.memory_size(), "Bytes"));

        if (!item.driver_date().empty())
            group.AddParam("Driver Date", Value::String(item.driver_date()));

        if (!item.driver_version().empty())
            group.AddParam("Driver Version", Value::String(item.driver_version()));

        if (!item.driver_provider().empty())
            group.AddParam("Driver Provider", Value::String(item.driver_provider()));
    }
}

std::string CategoryVideoAdapters::Serialize()
{
    proto::VideoAdapters message;

    for (VideoAdapterEnumarator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::VideoAdapters::Item* item = message.add_item();

        item->set_description(enumerator.GetDescription());
        item->set_adapter_string(enumerator.GetAdapterString());
        item->set_bios_string(enumerator.GetBIOSString());
        item->set_chip_type(enumerator.GetChipString());
        item->set_dac_type(enumerator.GetDACType());
        item->set_driver_date(enumerator.GetDriverDate());
        item->set_driver_version(enumerator.GetDriverVersion());
        item->set_driver_provider(enumerator.GetDriverVendor());
        item->set_memory_size(enumerator.GetMemorySize());
    }

    return message.SerializeAsString();
}

//
// CategoryMonitor
//

const char* CategoryMonitor::Name() const
{
    return "Monitor";
}

Category::IconId CategoryMonitor::Icon() const
{
    return IDI_MONITOR;
}

const char* CategoryMonitor::Guid() const
{
    return "{281100E4-88ED-4AE2-BC4A-3A37282BBAB5}";
}

void CategoryMonitor::Parse(Table& table, const std::string& data)
{
    proto::Monitors message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Monitors::Item& item = message.item(index);

        Group group = table.AddGroup(item.system_name());

        if (!item.monitor_name().empty())
            group.AddParam("Monitor Name", Value::String(item.monitor_name()));

        if (!item.manufacturer_name().empty())
            group.AddParam("Manufacturer Name", Value::String(item.manufacturer_name()));

        if (!item.monitor_id().empty())
            group.AddParam("Monitor ID", Value::String(item.monitor_id()));

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (item.edid_version() != 0)
            group.AddParam("EDID Version", Value::Number(item.edid_version()));

        if (item.edid_revision() != 0)
            group.AddParam("EDID Revision", Value::Number(item.edid_revision()));

        if (item.week_of_manufacture() != 0)
        {
            group.AddParam("Week Of Manufacture", Value::Number(item.week_of_manufacture()));
        }

        if (item.year_of_manufacture() != 0)
        {
            group.AddParam("Year Of Manufacture", Value::Number(item.year_of_manufacture()));
        }

        if (item.gamma() != 0.0)
            group.AddParam("Gamma", Value::FormattedString("%.2f", item.gamma()));

        if (item.max_horizontal_image_size() != 0)
        {
            group.AddParam("Horizontal Image Size",
                           Value::Number(item.max_horizontal_image_size(), "cm"));
        }

        if (item.max_vertical_image_size() != 0)
        {
            group.AddParam("Vertical Image Size",
                           Value::Number(item.max_vertical_image_size(), "cm"));
        }
        if (item.max_horizontal_image_size() != 0 && item.max_vertical_image_size() != 0)
        {
            // Calculate the monitor diagonal by the Pythagorean theorem and translate from
            // centimeters to inches.
            double diagonal_size =
                sqrt((item.max_horizontal_image_size() * item.max_horizontal_image_size()) +
                (item.max_vertical_image_size() * item.max_vertical_image_size())) / 2.54;

            group.AddParam("Diagonal Size", Value::Number(diagonal_size, "\""));
        }

        if (item.horizontal_resolution() != 0)
        {
            group.AddParam("Horizontal Resolution",
                           Value::Number(item.horizontal_resolution(), "px"));
        }

        if (item.vertical_resoulution() != 0)
        {
            group.AddParam("Vertical Resolution",
                           Value::Number(item.vertical_resoulution(), "px"));
        }

        if (item.min_horizontal_rate() != 0)
        {
            group.AddParam("Minimum Horizontal Frequency",
                           Value::Number(item.min_horizontal_rate(), "kHz"));
        }

        if (item.max_horizontal_rate() != 0)
        {
            group.AddParam("Maximum Horizontal Frequency",
                           Value::Number(item.max_horizontal_rate(), "kHz"));
        }

        if (item.min_vertical_rate() != 0)
        {
            group.AddParam("Minimum Vertical Frequency",
                           Value::Number(item.min_vertical_rate(), "Hz"));
        }

        if (item.max_vertical_rate() != 0)
        {
            group.AddParam("Maximum Vertical Frequency",
                           Value::Number(item.max_vertical_rate(), "Hz"));
        }

        if (item.pixel_clock() != 0.0)
        {
            group.AddParam("Pixel Clock",
                           Value::Number(item.pixel_clock(), "MHz"));
        }

        if (item.max_pixel_clock() != 0)
        {
            group.AddParam("Maximum Pixel Clock",
                           Value::Number(item.max_pixel_clock(), "MHz"));
        }

        group.AddParam("Input Signal Type",
                       Value::String(InputSignalTypeToString(item.input_signal_type())));

        {
            Group features_group = group.AddGroup("Supported Features");

            features_group.AddParam("Default GTF", Value::Bool(item.default_gtf_supported()));
            features_group.AddParam("Suspend", Value::Bool(item.suspend_supported()));
            features_group.AddParam("Standby", Value::Bool(item.standby_supported()));
            features_group.AddParam("Active Off", Value::Bool(item.active_off_supported()));
            features_group.AddParam("Preferred Timing Mode", Value::Bool(item.preferred_timing_mode_supported()));
            features_group.AddParam("sRGB", Value::Bool(item.srgb_supported()));
        }

        if (item.timings_size() > 0)
        {
            Group modes_group = group.AddGroup("Supported Video Modes");

            for (int mode = 0; mode < item.timings_size(); ++mode)
            {
                const proto::Monitors::Timing& timing = item.timings(mode);

                modes_group.AddParam(StringPrintf("%dx%d", timing.width(), timing.height()),
                                 Value::Number(timing.frequency(), "Hz"));
            }
        }
    }
}

std::string CategoryMonitor::Serialize()
{
    proto::Monitors message;

    for (MonitorEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        std::unique_ptr<Edid> edid = enumerator.GetEDID();
        if (!edid)
            continue;

        proto::Monitors::Item* item = message.add_item();

        std::string system_name = enumerator.GetFriendlyName();

        if (system_name.empty())
            system_name = enumerator.GetDescription();

        item->set_system_name(system_name);
        item->set_monitor_name(edid->GetMonitorName());
        item->set_manufacturer_name(edid->GetManufacturerName());
        item->set_monitor_id(edid->GetMonitorId());
        item->set_serial_number(edid->GetSerialNumber());
        item->set_edid_version(edid->GetEdidVersion());
        item->set_edid_revision(edid->GetEdidRevision());
        item->set_week_of_manufacture(edid->GetWeekOfManufacture());
        item->set_year_of_manufacture(edid->GetYearOfManufacture());
        item->set_max_horizontal_image_size(edid->GetMaxHorizontalImageSize());
        item->set_max_vertical_image_size(edid->GetMaxVerticalImageSize());
        item->set_horizontal_resolution(edid->GetHorizontalResolution());
        item->set_vertical_resoulution(edid->GetVerticalResolution());
        item->set_gamma(edid->GetGamma());
        item->set_max_horizontal_rate(edid->GetMaxHorizontalRate());
        item->set_min_horizontal_rate(edid->GetMinHorizontalRate());
        item->set_max_vertical_rate(edid->GetMaxVerticalRate());
        item->set_min_vertical_rate(edid->GetMinVerticalRate());
        item->set_pixel_clock(edid->GetPixelClock());
        item->set_max_pixel_clock(edid->GetMaxSupportedPixelClock());

        switch (edid->GetInputSignalType())
        {
            case Edid::INPUT_SIGNAL_TYPE_DIGITAL:
                item->set_input_signal_type(proto::Monitors::INPUT_SIGNAL_TYPE_DIGITAL);
                break;

            case Edid::INPUT_SIGNAL_TYPE_ANALOG:
                item->set_input_signal_type(proto::Monitors::INPUT_SIGNAL_TYPE_ANALOG);
                break;

            default:
                break;
        }

        uint8_t supported_features = edid->GetFeatureSupport();

        if (supported_features & Edid::FEATURE_SUPPORT_DEFAULT_GTF_SUPPORTED)
            item->set_default_gtf_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_SUSPEND)
            item->set_suspend_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_STANDBY)
            item->set_standby_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_ACTIVE_OFF)
            item->set_active_off_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_PREFERRED_TIMING_MODE)
            item->set_preferred_timing_mode_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_SRGB)
            item->set_srgb_supported(true);

        auto add_timing = [&](int width, int height, int freq)
        {
            proto::Monitors::Timing* timing = item->add_timings();

            timing->set_width(width);
            timing->set_height(height);
            timing->set_frequency(freq);
        };

        uint8_t estabilished_timings1 = edid->GetEstabilishedTimings1();

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_800X600_60HZ)
            add_timing(800, 600, 60);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_800X600_56HZ)
            add_timing(800, 600, 56);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_75HZ)
            add_timing(640, 480, 75);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_72HZ)
            add_timing(640, 480, 72);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_67HZ)
            add_timing(640, 480, 67);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_60HZ)
            add_timing(640, 480, 60);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_720X400_88HZ)
            add_timing(720, 400, 88);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_720X400_70HZ)
            add_timing(720, 400, 70);

        uint8_t estabilished_timings2 = edid->GetEstabilishedTimings2();

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1280X1024_75HZ)
            add_timing(1280, 1024, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_75HZ)
            add_timing(1024, 768, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_70HZ)
            add_timing(1024, 768, 70);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_60HZ)
            add_timing(1024, 768, 60);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_87HZ)
            add_timing(1024, 768, 87);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_832X624_75HZ)
            add_timing(832, 624, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_800X600_75HZ)
            add_timing(800, 600, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_800X600_72HZ)
            add_timing(800, 600, 72);

        uint8_t manufacturer_timings = edid->GetManufacturersTimings();

        if (manufacturer_timings & Edid::MANUFACTURERS_TIMINGS_1152X870_75HZ)
            add_timing(1152, 870, 75);

        for (int index = 0; index < edid->GetStandardTimingsCount(); ++index)
        {
            int width, height, freq;

            if (edid->GetStandardTimings(index, width, height, freq))
                add_timing(width, height, freq);
        }
    }

    return message.SerializeAsString();
}

// static
const char* CategoryMonitor::InputSignalTypeToString(proto::Monitors::InputSignalType value)
{
    switch (value)
    {
        case proto::Monitors::INPUT_SIGNAL_TYPE_DIGITAL:
            return "Digital";

        case proto::Monitors::INPUT_SIGNAL_TYPE_ANALOG:
            return "Analog";

        default:
            return "Unknown";
    }
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
// CategoryPrinters
//

const char* CategoryPrinters::Name() const
{
    return "Printers";
}

Category::IconId CategoryPrinters::Icon() const
{
    return IDI_PRINTER;
}

const char* CategoryPrinters::Guid() const
{
    return "{ACBDCE39-CE38-4A79-9626-8C8BA2E3A26A}";
}

void CategoryPrinters::Parse(Table& table, const std::string& data)
{
    proto::Printers message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Printers::Item& item = message.item(index);

        Group group = table.AddGroup(item.name());

        group.AddParam("Default Printer", Value::Bool(item.is_default()));
        group.AddParam("Shared Printer", Value::Bool(item.is_shared()));
        group.AddParam("Port", Value::String(item.port_name()));
        group.AddParam("Driver", Value::String(item.driver_name()));
        group.AddParam("Device Name", Value::String(item.device_name()));
        group.AddParam("Print Processor", Value::String(item.print_processor()));
        group.AddParam("Data Type", Value::String(item.data_type()));
        group.AddParam("Print Jobs Queued", Value::Number(item.jobs_count()));

        if (item.paper_width())
        {
            group.AddParam("Paper Width", Value::Number(item.paper_width(), "mm"));
        }

        if (item.paper_length())
        {
            group.AddParam("Paper Length", Value::Number(item.paper_length(), "mm"));
        }

        if (item.print_quality())
        {
            group.AddParam("Print Quality", Value::Number(item.print_quality(), "dpi"));
        }

        switch (item.orientation())
        {
            case proto::Printers::Item::ORIENTATION_LANDSCAPE:
                group.AddParam("Orientation", Value::String("Landscape"));
                break;

            case proto::Printers::Item::ORIENTATION_PORTRAIT:
                group.AddParam("Orientation", Value::String("Portrait"));
                break;

            default:
                group.AddParam("Orientation", Value::String("Unknown"));
                break;
        }
    }
}

std::string CategoryPrinters::Serialize()
{
    proto::Printers message;

    for (PrinterEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Printers::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_is_default(enumerator.IsDefault());
        item->set_is_shared(enumerator.IsShared());
        item->set_share_name(enumerator.GetShareName());
        item->set_port_name(enumerator.GetPortName());
        item->set_driver_name(enumerator.GetDriverName());
        item->set_device_name(enumerator.GetDeviceName());
        item->set_print_processor(enumerator.GetPrintProcessor());
        item->set_data_type(enumerator.GetDataType());
        item->set_server_name(enumerator.GetServerName());
        item->set_location(enumerator.GetLocation());
        item->set_comment(enumerator.GetComment());
        item->set_jobs_count(enumerator.GetJobsCount());
        item->set_paper_width(enumerator.GetPaperWidth());
        item->set_paper_length(enumerator.GetPaperLength());
        item->set_print_quality(enumerator.GetPrintQuality());

        switch (enumerator.GetOrientation())
        {
            case PrinterEnumerator::Orientation::PORTRAIT:
                item->set_orientation(proto::Printers::Item::ORIENTATION_PORTRAIT);
                break;

            case PrinterEnumerator::Orientation::LANDSCAPE:
                item->set_orientation(proto::Printers::Item::ORIENTATION_LANDSCAPE);
                break;

            default:
                item->set_orientation(proto::Printers::Item::ORIENTATION_UNKNOWN);
                break;
        }
    }

    return message.SerializeAsString();
}

//
// CategoryPowerOptions
//

const char* CategoryPowerOptions::Name() const
{
    return "Power Options";
}

Category::IconId CategoryPowerOptions::Icon() const
{
    return IDI_POWER_SUPPLY;
}

const char* CategoryPowerOptions::Guid() const
{
    return "{42E04A9E-36F7-42A1-BCDA-F3ED70112DFF}";
}

void CategoryPowerOptions::Parse(Table& table, const std::string& data)
{
    proto::PowerOptions message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    const char* power_source;
    switch (message.power_source())
    {
        case proto::PowerOptions::POWER_SOURCE_DC_BATTERY:
            power_source = "DC Battery";
            break;

        case proto::PowerOptions::POWER_SOURCE_AC_LINE:
            power_source = "AC Line";
            break;

        default:
            power_source = "Unknown";
            break;
    }

    table.AddParam("Power Source", Value::String(power_source));

    const char* battery_status;
    switch (message.battery_status())
    {
        case proto::PowerOptions::BATTERY_STATUS_HIGH:
            battery_status = "High";
            break;

        case proto::PowerOptions::BATTERY_STATUS_LOW:
            battery_status = "Low";
            break;

        case proto::PowerOptions::BATTERY_STATUS_CRITICAL:
            battery_status = "Critical";
            break;

        case proto::PowerOptions::BATTERY_STATUS_CHARGING:
            battery_status = "Charging";
            break;

        case proto::PowerOptions::BATTERY_STATUS_NO_BATTERY:
            battery_status = "No Battery";
            break;

        default:
            battery_status = "Unknown";
            break;
    }

    table.AddParam("Battery Status", Value::String(battery_status));

    if (message.battery_status() != proto::PowerOptions::BATTERY_STATUS_NO_BATTERY &&
        message.battery_status() != proto::PowerOptions::BATTERY_STATUS_UNKNOWN)
    {
        table.AddParam("Battery Life Percent",
                       Value::Number(message.battery_life_percent(), "%"));

        table.AddParam("Full Battery Life Time",
                       Value::Number(message.full_battery_life_time(), "s"));

        table.AddParam("Remaining Battery Life Time",
                       Value::Number(message.remaining_battery_life_time(), "s"));
    }

    for (int index = 0; index < message.battery_size(); ++index)
    {
        const proto::PowerOptions::Battery& battery = message.battery(index);

        Group group = table.AddGroup(StringPrintf("Battery #%d", index + 1));

        if (!battery.device_name().empty())
            group.AddParam("Device Name", Value::String(battery.device_name()));

        if (!battery.manufacturer().empty())
            group.AddParam("Manufacturer", Value::String(battery.manufacturer()));

        if (!battery.manufacture_date().empty())
            group.AddParam("Manufacture Date", Value::String(battery.manufacture_date()));

        if (!battery.unique_id().empty())
            group.AddParam("Unique Id", Value::String(battery.unique_id()));

        if (!battery.serial_number().empty())
            group.AddParam("Serial Number", Value::String(battery.serial_number()));

        if (!battery.temperature().empty())
            group.AddParam("Tempareture", Value::String(battery.temperature()));

        if (battery.design_capacity() != 0)
        {
            group.AddParam("Design Capacity", Value::Number(battery.design_capacity(), "mWh"));
        }

        if (!battery.type().empty())
            group.AddParam("Type", Value::String(battery.type()));

        if (battery.full_charged_capacity() != 0)
        {
            group.AddParam("Full Charged Capacity",
                           Value::Number(battery.full_charged_capacity(), "mWh"));
        }

        if (battery.depreciation() != 0)
            group.AddParam("Depreciation", Value::Number(battery.depreciation(), "%"));

        if (battery.current_capacity() != 0)
            group.AddParam("Current Capacity", Value::Number(battery.current_capacity(), "mWh"));

        if (battery.voltage() != 0)
            group.AddParam("Voltage", Value::Number(battery.voltage(), "mV"));

        if (battery.state() != 0)
        {
            Group state_group = group.AddGroup("State");

            if (battery.state() & proto::PowerOptions::Battery::STATE_CHARGING)
                state_group.AddParam("Charging", Value::String("Yes"));

            if (battery.state() & proto::PowerOptions::Battery::STATE_CRITICAL)
                state_group.AddParam("Critical", Value::String("Yes"));

            if (battery.state() & proto::PowerOptions::Battery::STATE_DISCHARGING)
                state_group.AddParam("Discharging", Value::String("Yes"));

            if (battery.state() & proto::PowerOptions::Battery::STATE_POWER_ONLINE)
                state_group.AddParam("Power OnLine", Value::String("Yes"));
        }
    }
}

std::string CategoryPowerOptions::Serialize()
{
    proto::PowerOptions message;

    SYSTEM_POWER_STATUS power_status;
    memset(&power_status, 0, sizeof(power_status));

    if (GetSystemPowerStatus(&power_status))
    {
        switch (power_status.ACLineStatus)
        {
            case 0:
                message.set_power_source(proto::PowerOptions::POWER_SOURCE_DC_BATTERY);
                break;

            case 1:
                message.set_power_source(proto::PowerOptions::POWER_SOURCE_AC_LINE);
                break;

            default:
                break;
        }

        switch (power_status.BatteryFlag)
        {
            case 1:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_HIGH);
                break;

            case 2:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_LOW);
                break;

            case 4:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_CRITICAL);
                break;

            case 8:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_CHARGING);
                break;

            case 128:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_NO_BATTERY);
                break;

            default:
                break;
        }

        if (power_status.BatteryFlag != 128)
        {
            message.set_battery_life_percent(power_status.BatteryLifePercent);

            if (power_status.BatteryFullLifeTime != -1)
                message.set_full_battery_life_time(power_status.BatteryFullLifeTime);

            if (power_status.BatteryLifeTime != -1)
                message.set_remaining_battery_life_time(power_status.BatteryLifeTime);
        }
    }

    for (BatteryEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::PowerOptions::Battery* battery = message.add_battery();

        battery->set_device_name(enumerator.GetDeviceName());
        battery->set_manufacturer(enumerator.GetManufacturer());
        battery->set_manufacture_date(enumerator.GetManufactureDate());
        battery->set_unique_id(enumerator.GetUniqueId());
        battery->set_serial_number(enumerator.GetSerialNumber());
        battery->set_temperature(enumerator.GetTemperature());
        battery->set_design_capacity(enumerator.GetDesignCapacity());
        battery->set_type(enumerator.GetType());
        battery->set_full_charged_capacity(enumerator.GetFullChargedCapacity());
        battery->set_depreciation(enumerator.GetDepreciation());
        battery->set_current_capacity(enumerator.GetCurrentCapacity());
        battery->set_voltage(enumerator.GetVoltage());

        const uint32_t state = enumerator.GetState();

        if (state & BatteryEnumerator::STATE_CHARGING)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_CHARGING);
        }

        if (state & BatteryEnumerator::STATE_CRITICAL)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_CRITICAL);
        }

        if (state & BatteryEnumerator::STATE_DISCHARGING)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_DISCHARGING);
        }

        if (state & BatteryEnumerator::STATE_POWER_ONLINE)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_POWER_ONLINE);
        }
    }

    return message.SerializeAsString();
}

//
// CategoryWindowsDevices
//

const char* CategoryWindowsDevices::Name() const
{
    return "Windows Devices";
}

Category::IconId CategoryWindowsDevices::Icon() const
{
    return IDI_PCI;
}

const char* CategoryWindowsDevices::Guid() const
{
    return "{22C4F1A6-67F2-4445-B807-9D39E1A80636}";
}

void CategoryWindowsDevices::Parse(Table& table, const std::string& data)
{
    proto::WindowsDevices message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Device Name", 200)
                     .AddColumn("Driver Version", 90)
                     .AddColumn("Driver Date", 80)
                     .AddColumn("Driver Vendor", 90)
                     .AddColumn("Device ID", 200));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::WindowsDevices::Item& item = message.item(index);

        Row row = table.AddRow();

        if (!item.friendly_name().empty())
            row.AddValue(Value::String(item.friendly_name()));
        else if (!item.description().empty())
            row.AddValue(Value::String(item.description()));
        else
            row.AddValue(Value::String("Unknown Device"));

        row.AddValue(Value::String(item.driver_version()));
        row.AddValue(Value::String(item.driver_date()));
        row.AddValue(Value::String(item.driver_vendor()));
        row.AddValue(Value::String(item.device_id()));
    }
}

std::string CategoryWindowsDevices::Serialize()
{
    proto::WindowsDevices message;

    for (DeviceEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::WindowsDevices::Item* item = message.add_item();

        item->set_friendly_name(enumerator.GetFriendlyName());
        item->set_description(enumerator.GetDescription());
        item->set_driver_version(enumerator.GetDriverVersion());
        item->set_driver_date(enumerator.GetDriverDate());
        item->set_driver_vendor(enumerator.GetDriverVendor());
        item->set_device_id(enumerator.GetDeviceID());
    }

    return message.SerializeAsString();
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
